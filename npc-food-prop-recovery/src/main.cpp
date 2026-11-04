#include <RE/Skyrim.h>
#include <RE/A/AnimationObjects.h>
#include <RE/B/BSResponse.h>
#include <RE/I/IHandlerFunctor.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include <utility>
#include "RecoveryPolicy.h"

namespace {
using namespace std::chrono_literals;
using Source = RE::BSTEventSource<RE::BSAnimationGraphEvent>;
using Sink = RE::BSTEventSink<RE::BSAnimationGraphEvent>;
using Hook = RE::BSEventNotifyControl (*)(Sink*, const RE::BSAnimationGraphEvent*, Source*);
REL::Relocation<Hook> original;
bool enabled = true, repair = true, verbose = true;
std::atomic_bool ready{false}, session{false}, taskPending{false};
std::atomic_uint64_t epoch{0};
std::jthread worker;

std::string lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 'a' - 'A');
    return out;
}
void readSettings() {
    std::ifstream input("Data/SKSE/Plugins/NPCFoodPropRecovery.ini");
    for (std::string line; std::getline(input, line);) {
        if (auto comment = line.find(';'); comment != std::string::npos) line.resize(comment);
        line.erase(std::remove_if(line.begin(), line.end(),
            [](char c) { return c == ' ' || c == '\t' || c == '\r'; }), line.end());
        const auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        const auto key = lower(line.substr(0, pos));
        const auto value = line.substr(pos + 1);
        if (value != "0" && value != "1") continue;
        if (key == "enabled") enabled = value == "1";
        else if (key == "repair") repair = value == "1";
        else if (key == "logevents") verbose = value == "1";
    }
}

struct Event {
    RE::ObjectRefHandle handle;
    std::uintptr_t source = 0;
    std::string tag, payload;
    std::uint64_t epoch = 0;
};
struct Watch {
    RE::ObjectRefHandle handle;
    std::uintptr_t source = 0;
    RE::TESObjectANIO* form = nullptr; // TES forms live until process termination.
    std::string loadPayload, drawTag, drawPayload;
    foodprop::Policy policy;
    foodprop::Observation previous = foodprop::Observation::Ambiguous;
    bool reported = false;
};
std::mutex eventMutex;
std::deque<Event> events;
bool overflow = false;
std::unordered_map<std::string, RE::TESObjectANIO*> foodForms;
std::unordered_map<std::uint32_t, Watch> watches; // main game thread only
std::uint64_t activeTime = 0, localEpoch = 0, nextSummary = 0, eventCount = 0, requests = 0;

bool relevant(std::string_view tag) {
    return tag.starts_with("AnimObj") || tag == "NPCAnimObjDraw" ||
        tag == "IdleStop" || tag == "IdleForceDefaultState" ||
        tag == "MoveStart" || tag == "JumpUp" || tag == "WeaponDraw";
}
RE::BSEventNotifyControl animationEvent(Sink* sink,
    const RE::BSAnimationGraphEvent* event, Source* source) {
    // Always preserve the original event handler and its return value.
    const auto result = original(sink, event, source);
    if (!ready || !session || !event || !event->holder || !source) return result;
    const std::string_view tag(event->tag.c_str());
    if (!relevant(tag)) return result;
    auto* actor = const_cast<RE::TESObjectREFR*>(event->holder)->As<RE::Actor>();
    if (!actor || actor->IsPlayerRef()) return result;
    Event saved{actor->GetHandle(), reinterpret_cast<std::uintptr_t>(source),
        std::string(tag), std::string(event->payload.c_str()), epoch.load()};
    std::scoped_lock lock(eventMutex);
    if (events.size() >= 4096) {
        events.clear();
        overflow = true; // Lost lifecycle events => abandon all tracked sequences.
    }
    events.push_back(std::move(saved));
    return result;
}
bool eventsPending(std::uint32_t handle) {
    std::scoped_lock lock(eventMutex);
    return overflow || std::any_of(events.begin(), events.end(), [handle](const Event& e) {
        return e.handle.native_handle() == handle;
    });
}
void drainEvents() {
    std::deque<Event> incoming;
    bool dropped;
    {
        std::scoped_lock lock(eventMutex);
        incoming.swap(events);
        dropped = std::exchange(overflow, false);
    }
    if (dropped) {
        watches.clear();
        SKSE::log::warn("Event queue overflow: discarded all recovery state");
        return;
    }
    for (const auto& e : incoming) {
        if (e.epoch != localEpoch) continue;
        ++eventCount;
        auto ref = e.handle.get();
        auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
        if (!actor || actor->IsPlayerRef()) continue;
        const auto key = e.handle.native_handle();
        const auto tag = lower(e.tag);
        if (verbose && e.tag != "MoveStart")
            SKSE::log::info("event actor={:08X} tag={} payload='{}'",
                actor->GetFormID(), e.tag, e.payload);
        if (tag == "animobjload") {
            // A different prop request invalidates the old food sequence.
            watches.erase(key);
            const auto it = foodForms.find(lower(e.payload));
            if (it == foodForms.end() || watches.size() >= 256) continue;
            Watch watch;
            watch.handle = e.handle;
            watch.source = e.source;
            watch.form = it->second;
            watch.loadPayload = e.payload;
            watch.policy = foodprop::Policy(activeTime);
            watches.emplace(key, std::move(watch));
        } else if (tag == "animobjdraw" || tag == "npcanimobjdraw") {
            const auto it = watches.find(key);
            if (it == watches.end()) continue;
            if (it->second.source != e.source) { watches.erase(it); continue; }
            it->second.drawTag = e.tag;
            it->second.drawPayload = e.payload;
            it->second.policy.onDraw();
        } else {
            // Stop, hide, unequip, unknown AnimObj event, locomotion or combat:
            // fail closed rather than leave a remembered prop on an idle NPC.
            watches.erase(key);
        }
    }
}

bool sourceIsCurrent(RE::Actor* actor, std::uintptr_t source) {
    RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
    if (!actor->GetAnimationGraphManager(manager) || !manager) return false;
    RE::BSSpinLockGuard lock(manager->GetRuntimeData().updateLock);
    for (const auto& graph : manager->graphs) {
        if (graph && reinterpret_cast<std::uintptr_t>(
            graph->GetEventSource<RE::BSAnimationGraphEvent>()) == source) return true;
    }
    return false;
}
bool underRoot(RE::NiAVObject* node, RE::NiAVObject* root) {
    for (unsigned depth = 0; node && depth < 128; ++depth, node = node->parent)
        if (node == root) return true;
    return false;
}
const char* label(foodprop::Observation o) {
    switch (o) {
    case foodprop::Observation::Healthy: return "visible-attached";
    case foodprop::Observation::Missing: return "missing-or-detached";
    case foodprop::Observation::Hidden: return "attached-but-hidden";
    case foodprop::Observation::Pending: return "async-load-pending";
    case foodprop::Observation::Suppressed: return "engine-visible-false";
    default: return "ambiguous";
    }
}
foodprop::Observation observe(const Watch& watch, RE::NiAVObject* root) {
    auto* registry = RE::AnimationObjects::GetSingleton();
    if (!registry) return foodprop::Observation::Ambiguous;
    unsigned matches = 0;
    bool visible = false, pending = false;
    RE::NiPointer<RE::NiAVObject> attached;
    {
        RE::BSSpinLockGuard lock(registry->lock);
        for (const auto& entry : registry->animationObjects) {
            const auto* object = entry.get();
            if (!object || object->handle != watch.handle || object->object != watch.form) continue;
            ++matches;
            visible = object->visible;
            pending = object->queuedObject.get() != nullptr;
            attached = object->attachedObject;
        }
    }
    if (matches > 1) return foodprop::Observation::Ambiguous;
    if (!matches) return foodprop::Observation::Missing;
    if (pending) return foodprop::Observation::Pending;
    if (!visible) return foodprop::Observation::Suppressed;
    if (!attached || !underRoot(attached.get(), root)) return foodprop::Observation::Missing;
    return attached->GetAppCulled() ? foodprop::Observation::Hidden : foodprop::Observation::Healthy;
}
bool execute(RE::Actor* actor, const std::string& tag, const std::string& payload) {
    auto* process = actor->GetActorRuntimeData().currentProcess;
    auto response = process && process->middleHigh ? process->middleHigh->animResponse :
        RE::BSTSmartPointer<RE::AnimResponse>{};
    if (!response) return false;
    const auto handler = response->GetHandler(RE::BSFixedString(tag.c_str()));
    if (!handler) return false;
    // Invoke only the existing engine prop handler, not a behavior transition,
    // quest reset, inventory operation or animation restart.
    return handler->ExecuteHandler(*actor, RE::BSFixedString(payload.c_str()));
}
void tick(std::uint64_t scheduledEpoch) {
    if (!ready || !session || scheduledEpoch != epoch.load()) return;
    if (localEpoch != scheduledEpoch) {
        watches.clear();
        localEpoch = scheduledEpoch;
        activeTime = 0;
        nextSummary = 0;
        eventCount = 0;
        requests = 0;
        SKSE::log::info("New session: cleared all transient tracking");
    }
    auto* ui = RE::UI::GetSingleton();
    if (!ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME)) return;
    activeTime += 500;
    drainEvents();
    for (auto it = watches.begin(); it != watches.end();) {
        auto& watch = it->second;
        auto ref = watch.handle.get();
        auto* actor = ref ? ref->As<RE::Actor>() : nullptr;
        auto* root = actor ? actor->Get3D(false) : nullptr;
        if (!actor || actor->IsPlayerRef() || actor->IsDead() || actor->IsDisabled() ||
            !root || !sourceIsCurrent(actor, watch.source) || watch.policy.expired(activeTime)) {
            it = watches.erase(it);
            continue;
        }
        const bool eligible = !actor->IsInCombat() && !root->GetAppCulled();
        const auto obs = observe(watch, root);
        if (!watch.reported || watch.previous != obs) {
            if (verbose) SKSE::log::info("state actor={:08X} prop={} drawn={} status={}",
                actor->GetFormID(), watch.loadPayload, watch.policy.drawn, label(obs));
            watch.reported = true;
            watch.previous = obs;
        }
        if (watch.policy.awaitingResult && obs == foodprop::Observation::Healthy)
            SKSE::log::info("Recovery observed: actor={:08X} prop={} is now attached and visible",
                actor->GetFormID(), watch.loadPayload);
        // Pending lifecycle events invalidate the observation; process them
        // before deciding whether a retry is still appropriate.
        if (eventsPending(it->first)) { ++it; continue; }
        auto action = watch.policy.observe(obs, activeTime, eligible, repair);
        if (action != foodprop::Action::None) {
            // Recheck immediately before using engine handlers.
            if (observe(watch, root) != obs || eventsPending(it->first)) {
                ++it; continue;
            }
            bool loadOK = true;
            if (action == foodprop::Action::ReloadAndDraw)
                loadOK = execute(actor, "AnimObjLoad", watch.loadPayload);
            bool drawOK = false;
            if (loadOK && !eventsPending(it->first))
                drawOK = execute(actor, watch.drawTag, watch.drawPayload);
            ++requests;
            SKSE::log::info("Recovery requested: actor={:08X} prop={} attempt={} action={} load={} draw={}",
                actor->GetFormID(), watch.loadPayload, watch.policy.attempts,
                action == foodprop::Action::DrawOnly ? "draw" : "load+draw", loadOK, drawOK);
        }
        ++it;
    }
    if (verbose && activeTime >= nextSummary) {
        SKSE::log::info("Summary: tracked={} events={} recovery_requests={} repair={}",
            watches.size(), eventCount, requests, repair);
        nextSummary = activeTime + 30000;
    }
}
void initialize() {
    if (!enabled) { SKSE::log::info("Disabled by INI"); return; }
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    for (auto* form : data->GetFormArray<RE::TESObjectANIO>()) {
        if (!form) continue;
        auto id = lower(form->GetFormEditorID());
        if (id == "animobjectbread" || id == "animobjecttankard") {
            foodForms.emplace(id, form);
            SKSE::log::info("Recognized food prop: {} {:08X}", id, form->GetFormID());
        }
    }
    if (foodForms.empty()) { SKSE::log::error("No supported food props; patch remains inactive"); return; }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_Character[2]};
    original = table.write_vfunc(0x1, animationEvent);
    ready = true;
    worker = std::jthread([](std::stop_token stop) {
        while (!stop.stop_requested()) {
            std::this_thread::sleep_for(500ms);
            if (!ready || !session || taskPending.exchange(true)) continue;
            const auto generation = epoch.load();
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) { taskPending = false; continue; }
            tasks->AddTask([generation] {
                tick(generation);
                taskPending = false;
            });
        }
    });
    SKSE::log::info("Ready: vanilla NPC bread/tankards; repair={} log_events={}", repair, verbose);
}
void message(SKSE::MessagingInterface::Message* m) {
    switch (m->type) {
    case SKSE::MessagingInterface::kDataLoaded: initialize(); break;
    case SKSE::MessagingInterface::kPreLoadGame: session = false; ++epoch; break;
    case SKSE::MessagingInterface::kNewGame: ++epoch; session = true; break;
    case SKSE::MessagingInterface::kPostLoadGame: ++epoch; session = m->data != nullptr; break;
    default: break;
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({0,1,0,0});
    data.PluginName("NPCFoodPropRecovery");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true);
    data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}});
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "NPCFoodPropRecovery.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    readSettings();
    SKSE::log::info("NPC Food Prop Recovery 0.1.0 test 1; Steam Skyrim 1.6.1170; experimental");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
