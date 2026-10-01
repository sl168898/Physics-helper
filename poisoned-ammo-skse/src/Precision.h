#pragma once
#include "PrecisionCore.h"
#include "CriticalEntryGate.h"
#include <MinHook.h>
#include <mutex>

namespace precision
{
    inline RE::BGSPerk* perk{};
    inline bool available{}, trace{};
    // Supplied by the recipe owner; never hold its mutex across an engine call.
    using Lookup = bool (*)(RE::TESAmmo*, Shot&);
    using Active = bool (*)(std::uint64_t);
    inline Lookup lookup{};
    inline Active active{};
    inline std::mutex shotsMutex;
    inline ShotStore shots;
    inline thread_local const Frame* launching{};
    inline thread_local unsigned entryDepth{};

    inline void clear()
    {
        std::lock_guard lock(shotsMutex);
        shots.clear();
    }
    inline bool matches(RE::ArrowProjectile* p, const Shot& shot)
    {
        auto& runtime = p->GetProjectileRuntimeData();
        const auto shooter = runtime.shooter.get();
        const auto ammo = runtime.ammoSource;
        return shooter && shooter->GetFormID() == shot.shooter && runtime.weaponSource &&
            runtime.weaponSource->GetFormID() == shot.weapon && ammo &&
            (ammo->GetFormID() == shot.ammo || ammo->GetFormID() == shot.baseAmmo);
    }
    inline void remember(RE::ArrowProjectile* p, const Shot& shot)
    {
        if (!p || !active(shot.epoch) || !matches(p, shot)) return;
        std::lock_guard lock(shotsMutex);
        shots.remember(p->GetHandle().native_handle(), shot);
    }
    inline void loaded(RE::ArrowProjectile* p)
    {
        // Handle3DLoaded can run synchronously before Projectile::Launch returns.
        if (launching) remember(p, launching->shot);
    }
    class Scope
    {
        Frame frame{};
        FrameGuard guard{nullptr}; // Nested plain arrows never inherit a bolt.
    public:
        explicit Scope(RE::ArrowProjectile* p, RE::TESObjectREFR* target = nullptr)
        {
            if (!available || !p) return;
            std::optional<Shot> saved;
            {
                std::lock_guard lock(shotsMutex);
                saved = shots.find(p->GetHandle().native_handle());
            }
            if (!saved || !active(saved->epoch) || !matches(p, *saved)) return;
            frame = {*saved, target ? target->GetFormID() : 0};
            current = &frame;
        }
    };

    struct LaunchHook
    {
        static RE::ProjectileHandle* thunk(RE::ProjectileHandle* result, RE::Projectile::LaunchData& data) noexcept
        {
            Frame frame{};
            bool valid = available && data.shooter == RE::PlayerCharacter::GetSingleton() &&
                data.weaponSource && data.weaponSource->IsCrossbow() && data.ammoSource &&
                data.ammoSource->IsBolt() && !data.spell;
            auto actor = valid ? data.shooter->As<RE::Actor>() : nullptr;
            valid = valid && actor && actor->HasPerk(perk) && lookup(data.ammoSource, frame.shot);
            if (valid) {
                frame.shot.shooter = actor->GetFormID();
                frame.shot.weapon = data.weaponSource->GetFormID();
                frame.shot.stamina = actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kStamina);
                valid = std::isfinite(frame.shot.stamina);
            }
            struct LaunchGuard {
                const Frame* previous = launching;
                explicit LaunchGuard(const Frame* value) { launching = value; }
                ~LaunchGuard() { launching = previous; }
            } launchGuard(valid ? &frame : nullptr);
            FrameGuard guard(valid ? &frame : nullptr);
            auto returned = original(result, data);
            if (valid && returned) {
                const auto p = returned->get();
                if (p) remember(p->As<RE::ArrowProjectile>(), frame.shot);
                if (trace) SKSE::log::info("Alchemical Precision shot: handle={:08X}, weapon={:08X}, ammo={:08X}, stamina={}",
                    returned->native_handle(), frame.shot.weapon, frame.shot.ammo, frame.shot.stamina);
            }
            return returned;
        }
        inline static decltype(&thunk) original{};
    };
    struct KillHook
    {
        static void thunk(RE::ArrowProjectile* p)
        {
            {
                std::lock_guard lock(shotsMutex);
                shots.erase(p->GetHandle().native_handle());
            }
            original(p);
        }
        inline static REL::Relocation<decltype(thunk)> original;
    };
    struct CriticalHook
    {
        static void thunk(std::uint32_t entry, RE::Actor* owner, RE::TESObjectWEAP* weapon, RE::Actor* target, float* value)
        {
            struct DepthGuard { DepthGuard() { ++entryDepth; } ~DepthGuard() { --entryDepth; } } depth;
            // Skyrim resolves every existing perk first. Modify the same native
            // result, then let Skyrim do its one roll / apply its critical bonus.
            original(entry, owner, weapon, target, value);
            if (entryDepth != 1 || !available || !current || !value || !owner || !weapon ||
                owner != RE::PlayerCharacter::GetSingleton() || !active(current->shot.epoch) ||
                !eligible(current, current->shot.epoch, owner->GetFormID(), weapon->GetFormID(),
                    target ? target->GetFormID() : 0, owner->HasPerk(perk))) return;
            const float before = *value;
            *value = entry == 1 ? chance(before) : criticalBonus(before, current->shot.stamina);
            if (trace) SKSE::log::info("Alchemical Precision {}: target={:08X}, ordinary={}, enhanced={}, firing stamina={}",
                entry == 1 ? "chance" : "critical bonus", target ? target->GetFormID() : 0,
                before, *value, current->shot.stamina);
        }
        inline static decltype(&thunk) original{};
    };
    inline void install(RE::TESDataHandler* data, bool logging, Lookup resolve, Active isActive)
    {
        perk = data->LookupForm<RE::BGSPerk>(0x803, "CoatingMechanist.esp");
        if (!perk) { SKSE::log::info("Alchemical Precision unavailable: updated CoatingMechanist.esp absent"); return; }
        lookup = resolve; active = isActive; trace = logging;
        const auto init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
            SKSE::log::error("Alchemical Precision: MinHook initialization failed {}", int(init)); return;
        }
        void* entryAddress = reinterpret_cast<void*>(REL::Relocation<std::uintptr_t>{RELOCATION_ID(23073, 23526)}.address());
        void* launchAddress = reinterpret_cast<void*>(REL::Relocation<std::uintptr_t>{RELOCATION_ID(42928, 44108)}.address());
        void* gate = VirtualAlloc(nullptr, 34, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (!gate) { SKSE::log::error("Alchemical Precision: gate allocation failed"); return; }
        // MinHook requires an executable detour even before the hook is enabled.
        // Install an unreachable placeholder, then fill its trampoline address.
        const auto placeholder = criticalEntryGate(reinterpret_cast<std::uintptr_t>(&CriticalHook::thunk), 0);
        std::memcpy(gate, placeholder.data(), placeholder.size());
        DWORD previousProtection{};
        if (!VirtualProtect(gate, placeholder.size(), PAGE_EXECUTE_READ, &previousProtection)) {
            VirtualFree(gate, 0, MEM_RELEASE);
            SKSE::log::error("Alchemical Precision: executable gate protection failed"); return;
        }
        auto status = MH_CreateHook(entryAddress, gate, reinterpret_cast<void**>(&CriticalHook::original));
        if (status != MH_OK) {
            VirtualFree(gate, 0, MEM_RELEASE);
            SKSE::log::error("Alchemical Precision: critical entry hook failed {}", int(status)); return;
        }
        const auto code = criticalEntryGate(reinterpret_cast<std::uintptr_t>(&CriticalHook::thunk),
            reinterpret_cast<std::uintptr_t>(CriticalHook::original));
        bool launchCreated = false;
        bool ok = VirtualProtect(gate, code.size(), PAGE_READWRITE, &previousProtection) != 0;
        if (ok) {
            std::memcpy(gate, code.data(), code.size());
            ok = VirtualProtect(gate, code.size(), PAGE_EXECUTE_READ, &previousProtection) != 0 &&
                FlushInstructionCache(GetCurrentProcess(), gate, code.size()) != 0;
        }
        if (ok) {
            status = MH_CreateHook(launchAddress, reinterpret_cast<void*>(&LaunchHook::thunk),
                reinterpret_cast<void**>(&LaunchHook::original));
            launchCreated = status == MH_OK; ok = launchCreated;
        }
        if (ok) { status = MH_EnableHook(entryAddress); ok = status == MH_OK; }
        if (ok) { status = MH_EnableHook(launchAddress); ok = status == MH_OK; }
        if (!ok) {
            MH_DisableHook(entryAddress); MH_RemoveHook(entryAddress);
            if (launchCreated) { MH_DisableHook(launchAddress); MH_RemoveHook(launchAddress); }
            VirtualFree(gate, 0, MEM_RELEASE);
            SKSE::log::error("Alchemical Precision disabled: native hook setup failed {}", int(status)); return;
        }
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_ArrowProjectile[0]};
        KillHook::original = table.write_vfunc(0xA8, KillHook::thunk);
        available = true;
        SKSE::log::info("Alchemical Precision ready: native critical entry points 1/2; +25 points; bonus x(1+0.02*firing stamina); perk {:08X}", perk->GetFormID());
    }
}
