#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <Windows.h>
#include <MinHook.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <array>
#include <filesystem>
#include <vector>

// Diagnostic only. No reference increments/decrements other than forwarding
// the original call exactly once, no ownership pins, no save writes, no poll.
namespace {
using Manager = RE::BGSCreatedObjectManager;
using RefOperation = void (*)(Manager*, RE::AlchemyItem*);
using ClearOperation = void (*)(RE::AlchemyItem*);
// Microsoft x64 scalar-deleting-destructor ABI: this, unsigned flags, this result.
using DestroyOperation = void* (*)(RE::AlchemyItem*, unsigned int);
RefOperation originalIncrement{}, originalDecrement{};
ClearOperation originalClear{};
DestroyOperation originalDestroy{};
std::atomic_uint64_t serial{0};
std::atomic_uint32_t phase{0}; // 0 startup, 1 loading, 2 playing, 3 load failed
std::array<RE::FormID, 3> phialIDs{};
thread_local unsigned int hookDepth = 0;
std::vector<void*> installedTargets;

struct Depth {
    Depth() { ++hookDepth; }
    ~Depth() { --hookDepth; }
};
struct State {
    bool found{};
    bool queued{};
    RE::FormID id{};
    std::uint32_t references{};
};

// This tiny leaf is the only place probing a caller-provided object before
// finding it in the manager. Never follow its name/effects/virtual functions.
RE::FormID guardedID(const RE::AlchemyItem* item) noexcept {
    __try {
        return item ? item->formID : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

State stateOf(Manager* manager, const RE::AlchemyItem* item) {
    State s;
    if (!manager || !item) return s;
    RE::BSSpinLockGuard lock(manager->lock);
    const auto* magic = static_cast<const RE::MagicItem*>(item);
    for (const auto& [key, value] : manager->potions) {
        (void)key;
        if (value.magicItem == magic) {
            s.found = true;
            s.id = value.magicItem->GetFormID();
            s.references = value.refCount;
            break;
        }
    }
    if (!s.found) for (const auto& [key, value] : manager->poisons) {
        (void)key;
        if (value.magicItem == magic) {
            s.found = true;
            s.id = value.magicItem->GetFormID();
            s.references = value.refCount;
            break;
        }
    }
    s.queued = manager->queuedDeletes.contains(const_cast<RE::MagicItem*>(magic));
    return s;
}

void traceStack(std::uint64_t event) {
    void* frames[48]{};
    const auto n = CaptureStackBackTrace(1, 48, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        HMODULE mod{};
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(frames[i]), &mod)) {
            wchar_t file[1024]{};
            const auto length = GetModuleFileNameW(mod, file, 1024);
            const auto name = length && length < 1024 ? std::filesystem::path(file).filename().string() : "<module>";
            const auto rva = reinterpret_cast<std::uintptr_t>(frames[i]) - reinterpret_cast<std::uintptr_t>(mod);
            SKSE::log::info("STACK {} {:02} {}+{:X} absolute={:016X}", event, i, name, rva,
                reinterpret_cast<std::uintptr_t>(frames[i]));
        } else {
            SKSE::log::info("STACK {} {:02} <unmapped> absolute={:016X}", event, i,
                reinterpret_cast<std::uintptr_t>(frames[i]));
        }
    }
}

bool dynamic(RE::FormID id) { return (id & 0xFF000000u) == 0xFF000000u; }

void referenceOperation(Manager* manager, RE::AlchemyItem* item, RefOperation original, const char* operation) {
    if (hookDepth) { original(manager, item); return; }
    Depth depth;
    RE::FormID id{};
    std::uint64_t event{};
    try {
        const auto before = stateOf(manager, item);
        id = before.found ? before.id : guardedID(item);
        if (dynamic(id)) {
            event = ++serial;
            SKSE::log::info("BEGIN {} {} form={:08X} ptr={:016X} phase={} managed={} refs={} queued={} thread={}",
                event, operation, id, reinterpret_cast<std::uintptr_t>(item), phase.load(), before.found,
                before.references, before.queued, GetCurrentThreadId());
            traceStack(event); // Capture the release caller BEFORE entering the original.
        }
    } catch (...) { /* A diagnostic I/O/allocation failure must not skip the game call. */ }
    original(manager, item);
    // Only compare the pointer value after return; item could have been freed.
    if (event) try {
        const auto after = stateOf(manager, item);
        SKSE::log::info("END {} {} form={:08X} managed={} refs={} queued={}", event, operation,
            id, after.found, after.references, after.queued);
    } catch (...) {}
}
void increment(Manager* manager, RE::AlchemyItem* item) {
    referenceOperation(manager, item, originalIncrement, "INCREMENT");
}
void decrement(Manager* manager, RE::AlchemyItem* item) {
    referenceOperation(manager, item, originalDecrement, "DECREMENT");
}

void destructionTrace(RE::AlchemyItem* item, const char* operation, unsigned int flags) {
    const auto id = guardedID(item);
    if (!dynamic(id)) return;
    const auto event = ++serial;
    // Do NOT acquire the manager lock here. A destructor can be invoked while
    // the engine is tearing that manager down. Its call stack is sufficient.
    SKSE::log::info("DESTROY {} {} form={:08X} ptr={:016X} phase={} flags={:X} thread={}", event,
        operation, id, reinterpret_cast<std::uintptr_t>(item), phase.load(), flags, GetCurrentThreadId());
    traceStack(event);
}
void clear(RE::AlchemyItem* item) {
    try { destructionTrace(item, "CLEAR_DATA", 0); } catch (...) {}
    originalClear(item);
}
void* destroy(RE::AlchemyItem* item, unsigned int flags) {
    try { destructionTrace(item, "DELETING_DESTRUCTOR", flags); } catch (...) {}
    return originalDestroy(item, flags);
}

void checkpoint(const char* label) {
    auto* manager = Manager::GetSingleton();
    SKSE::log::info("CHECKPOINT {} phase={} thread={}", label, phase.load(), GetCurrentThreadId());
    if (!manager) return;
    struct Entry { RE::FormID id; std::uint32_t refs; bool queued; std::uintptr_t ptr; };
    std::vector<Entry> entries;
    {
        RE::BSSpinLockGuard lock(manager->lock);
        for (const auto* map : { &manager->potions, &manager->poisons }) {
            for (const auto& [key, value] : *map) {
                (void)key;
                if (value.magicItem) entries.push_back({value.magicItem->GetFormID(), value.refCount,
                    manager->queuedDeletes.contains(value.magicItem), reinterpret_cast<std::uintptr_t>(value.magicItem)});
            }
        }
    }
    for (const auto& entry : entries) SKSE::log::info("RECORD {} form={:08X} refs={} queued={} ptr={:016X}",
        label, entry.id, entry.refs, entry.queued, entry.ptr);
    SKSE::log::info("RECORD_COUNT {} {}", label, entries.size());
}

struct ContainerEvents final : RE::BSTEventSink<RE::TESContainerChangedEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESContainerChangedEvent* e,
            RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
        if (e && (dynamic(e->baseObj) || e->baseObj == phialIDs[0] || e->baseObj == phialIDs[1] || e->baseObj == phialIDs[2])) {
            // No GetInventory(), no object dereference. These IDs/counts belong
            // to the delivered event even if its old base form was just freed.
            SKSE::log::info("CONTAINER form={:08X} old={:08X} new={:08X} count={} phase={} thread={}",
                e->baseObj, e->oldContainer, e->newContainer, e->itemCount, phase.load(), GetCurrentThreadId());
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} containerEvents;

template <class F>
bool prepare(std::uintptr_t target, F replacement, F& original, const char* label) {
    auto* address = reinterpret_cast<void*>(target);
    const auto result = MH_CreateHook(address, reinterpret_cast<void*>(replacement), reinterpret_cast<void**>(&original));
    SKSE::log::info("HOOK {} address={:016X} status={}", label, target, MH_StatusToString(result));
    if (result != MH_OK) return false;
    installedTargets.push_back(address);
    return true;
}
void rollbackHooks() {
    for (auto* target : installedTargets) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    installedTargets.clear();
}
bool install() {
    const auto init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return false;
    // IDs from the AlchemyItem overloads, NOT enchantment reference methods:
    // adya/CommonLibSSE commit 3adc327... BGSCreatedObjectManager.cpp.
    REL::Relocation<std::uintptr_t> inc{REL::ID(36170)}, dec{REL::ID(36171)};
    REL::Relocation<std::uintptr_t> vtable{RE::VTABLE_AlchemyItem[0]};
    const auto* methods = reinterpret_cast<const std::uintptr_t*>(vtable.address());
    if (!prepare(inc.address(), increment, originalIncrement, "AlchemyIncrementRef") ||
        !prepare(dec.address(), decrement, originalDecrement, "AlchemyDecrementRef") ||
        !prepare(methods[0], destroy, originalDestroy, "AlchemyDeletingDestructor") ||
        !prepare(methods[5], clear, originalClear, "AlchemyClearData")) {
        rollbackHooks(); return false;
    }
    // Enable only this plugin's targets; never MH_ALL_HOOKS.
    for (auto* target : installedTargets) {
        if (MH_EnableHook(target) != MH_OK) { rollbackHooks(); return false; }
    }
    return true;
}

void message(SKSE::MessagingInterface::Message* event) {
    if (!event) return;
    switch (event->type) {
    case SKSE::MessagingInterface::kDataLoaded: {
        if (!install()) {
            SKSE::log::critical("DIAGNOSTIC INACTIVE: could not install every required hook; own hooks rolled back.");
            RE::DebugNotification("Phial trace failed to initialize. Check PhialLifetimeTrace.log.");
            return;
        }
        phialIDs[0] = 0x2C25A;
        auto* data = RE::TESDataHandler::GetSingleton();
        if (data) {
            constexpr auto file = "The White Phial - Tweaks and Enhancements.esp";
            if (auto* form = data->LookupForm<RE::AlchemyItem>(0x807, file)) phialIDs[1] = form->GetFormID();
            if (auto* form = data->LookupForm<RE::AlchemyItem>(0x80C, file)) phialIDs[2] = form->GetFormID();
        }
        if (auto* source = RE::ScriptEventSourceHolder::GetSingleton()) source->AddEventSink(&containerEvents);
        SKSE::log::info("READY: diagnostic only; no polling, pins, restoration, inventory edits or serialization.");
        checkpoint("DATA_LOADED");
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
        phase = 1; checkpoint("PRE_LOAD"); break;
    case SKSE::MessagingInterface::kPostLoadGame:
        phase = event->data ? 2 : 3; checkpoint(event->data ? "POST_LOAD_SUCCESS" : "POST_LOAD_FAILED"); break;
    case SKSE::MessagingInterface::kNewGame:
        phase = 2; checkpoint("NEW_GAME"); break;
    case SKSE::MessagingInterface::kSaveGame:
        checkpoint("SAVE_MESSAGE"); break;
    default: break;
    }
}
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({0,1,0,0}); data.PluginName("PhialLifetimeTrace");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}});
    return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    const auto directory = SKSE::log::log_directory();
    if (!directory) return false;
    const auto path = *directory / "PhialLifetimeTrace.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("PhialLifetimeTrace", sink));
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [thread %t] %v");
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("PhialLifetimeTrace 0.1.0; Steam Skyrim 1.6.1170; diagnostic, NOT a fix.");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
