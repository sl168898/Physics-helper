#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <mutex>
#include <vector>
#include <atomic>
#include "Policy.h"
namespace {
constexpr auto plugin="Boozy Bard - Safe Drinking.esp";
RE::EffectSetting* armor{};
RE::EffectSetting* legacyMagicka{};
using Add=bool(*)(RE::MagicTarget*,RE::MagicTarget::AddTargetData&);
Add original{};
std::uintptr_t hookedTable{};
std::recursive_mutex gate;
bool pending=false;
std::uint64_t serial=0;
bool warned=false;
bool protectedBase(const RE::EffectSetting* base) { return base && (base==armor || base==legacyMagicka); }
boozy::EffectView view(RE::ActiveEffect* effect) {
    if(!effect)return {};
    return {protectedBase(effect->GetBaseObject()),effect->flags.any(RE::ActiveEffect::Flag::kDispelled),effect->elapsedSeconds,effect->duration};
}
struct Snapshot { std::vector<RE::ActiveEffect*> ptrs; std::vector<boozy::EffectView> values; };
Snapshot snapshot(RE::MagicTarget* target) {
    Snapshot s;
    if(auto* list=target->GetActiveEffectList()) for(auto* e:*list) {
        if(!e)continue;
        s.ptrs.push_back(e);s.values.push_back(view(e));
    }
    return s;
}
void repair(const char* reason) {
    std::lock_guard lock(gate);
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(!player || !armor)return;
    auto* target=player->GetMagicTarget();if(!target)return;
    std::size_t removed=0;
    // Re-read the list after each Dispel. Never keep an iterator across a removal.
    for(std::size_t pass=0;pass<512;++pass) {
        auto s=snapshot(target);auto keep=boozy::oldest(s.values);
        if(!keep)break;
        RE::ActiveEffect* victim=nullptr;
        for(std::size_t i=0;i<s.values.size();++i)
            if(i!=*keep && boozy::occupies(s.values[i])) { victim=s.ptrs[i];break; }
        if(!victim)break;
        const auto source=victim->spell?victim->spell->GetFormID():0;
        const auto magnitude=victim->magnitude;
        // Native Dispel reverses the effect's recoverable actor-value modifiers.
        // Do not write player health/armor values ourselves.
        victim->Dispel(true);
        ++removed;
        SKSE::log::info("{}: dispelled duplicate alcohol source={:08X}, magnitude={}",reason,source,magnitude);
    }
    const auto s=snapshot(target);
    std::size_t count=0;for(const auto& e:s.values)if(boozy::occupies(e))++count;
    SKSE::log::info("{}: removed={}, remaining alcohol bonuses={}",reason,removed,count);
    if(count>1)SKSE::log::error("Cleanup left multiple live effects. Please provide this log.");
}
void queueRepair(const char* reason) {
    SKSE::GetTaskInterface()->AddTask([reason]{repair(reason);});
}
bool addTarget(RE::MagicTarget* target,RE::MagicTarget::AddTargetData& data) {
    auto* base=data.effect?data.effect->baseEffect:nullptr;
    if(!protectedBase(base))return original(target,data);
    std::lock_guard lock(gate);
    auto s=snapshot(target);
    if(boozy::blocked(true,pending,s.values)) {
        SKSE::log::info("Blocked extra alcohol bonus: source={:08X}, magnitude={}; pending={}",
            data.magicItem?data.magicItem->GetFormID():0,data.magnitude,pending);
        return false;
    }
    // Reserve the slot across recursive and rapid applications, even before an
    // accepted effect appears in the native list. This task is event-driven.
    pending=true;const auto ticket=++serial;
    bool accepted=original(target,data);
    SKSE::log::info("First alcohol application: accepted={}, source={:08X}, magnitude={}",
        accepted,data.magicItem?data.magicItem->GetFormID():0,data.magnitude);
    if(!accepted)pending=false;
    SKSE::GetTaskInterface()->AddTask([ticket]{
        {std::lock_guard lock(gate);if(ticket==serial)pending=false;}
        repair("drink");
    });
    return accepted;
}
bool install() {
    std::lock_guard lock(gate);
    auto* player=RE::PlayerCharacter::GetSingleton();
    if(!player || !armor)return false;
    auto* target=player->GetMagicTarget();
    if(!target || target->GetTargetStatsObject()!=player || !target->MagicTargetIsActor())return false;
    auto address=*reinterpret_cast<std::uintptr_t*>(target);
    bool playerTable=false;
    for(const auto& id:RE::VTABLE_PlayerCharacter) {
        if(REL::Relocation<std::uintptr_t>{id}.address()==address){playerTable=true;break;}
    }
    if(!playerTable) {SKSE::log::error("Unexpected player MagicTarget vtable; refusing to modify it");return false;}
    if(hookedTable) {
        if(address!=hookedTable || reinterpret_cast<Add*>(address)[1]!=addTarget) {
            SKSE::log::error("Another plugin replaced the player effect hook");return false;
        }
        return true;
    }
    REL::Relocation<std::uintptr_t> table{address};
    original=reinterpret_cast<Add>(table.write_vfunc(1,addTarget));
    hookedTable=address;
    SKSE::log::info("Installed player MagicTarget::AddTarget guard; original={:X}",reinterpret_cast<std::uintptr_t>(original));
    return original!=nullptr;
}
void activate(const char* reason) {
    if(install())queueRepair(reason);
    else if(!warned) {
        warned=true;SKSE::log::error("Alcohol application guard is not active");
        RE::DebugNotification("Boozy Bard guard failed to start. Check BoozyBardGuard.log.");
    }
}
class Events final:public RE::BSTEventSink<RE::TESMagicEffectApplyEvent> {
 RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* e,RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override {
    if(e && e->target.get()==RE::PlayerCharacter::GetSingleton() &&
      ((armor && e->magicEffect==armor->GetFormID()) || (legacyMagicka && e->magicEffect==legacyMagicka->GetFormID())))
        queueRepair("effect event");
    return RE::BSEventNotifyControl::kContinue;
 }
} events;
void message(SKSE::MessagingInterface::Message* m) {
    if(m->type==SKSE::MessagingInterface::kDataLoaded) {
        auto* data=RE::TESDataHandler::GetSingleton();
        armor=data->LookupForm<RE::EffectSetting>(0x805,plugin);
        legacyMagicka=data->LookupForm<RE::EffectSetting>(0x807,plugin);
        if(!armor || !legacyMagicka) {
            SKSE::log::error("Required Boozy Bard effect definitions missing; mod inactive");return;
        }
        SKSE::log::info("Resolved armor/health={:08X}, legacy magicka={:08X}",armor->GetFormID(),legacyMagicka->GetFormID());
        RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESMagicEffectApplyEvent>(&events);
        activate("data loaded");
    }
    if(m->type==SKSE::MessagingInterface::kPreLoadGame || m->type==SKSE::MessagingInterface::kNewGame) {
        std::lock_guard lock(gate);pending=false;++serial;
    }
    if(m->type==SKSE::MessagingInterface::kPostLoadGame || m->type==SKSE::MessagingInterface::kNewGame) {
        SKSE::GetTaskInterface()->AddTask([]{activate("save loaded");});
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
 SKSE::PluginVersionData d{};d.PluginVersion({1,3,0,0});d.PluginName("BoozyBardGuard");d.AuthorName("Physics-helper contributors");
 d.UsesAddressLibrary(true);d.UsesStructsPost629(true);d.CompatibleVersions({REL::Version{1,6,1170,0}});return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
 if(skse->RuntimeVersion()!=REL::Version{1,6,1170,0})return false;
 auto path=SKSE::log::log_directory();if(!path)return false;*path/="BoozyBardGuard.log";
 spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true)));
 spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);SKSE::Init(skse);
 SKSE::log::info("Boozy Bard Guard 1.3.0 beta; Skyrim Steam 1.6.1170; no periodic polling");
 return SKSE::GetMessagingInterface()->RegisterListener(message);
}
