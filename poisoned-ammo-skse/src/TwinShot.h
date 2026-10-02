#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <functional>

// Included after CommonLib and MinHook. Tests compile this exact adapter with
// engine doubles; the Windows build checks the real 1.6.1170 interfaces.
namespace twin
{
    constexpr float spreadRadians = 0.75f * 3.14159265358979323846f / 180.f;
    inline RE::BGSPerk* perk{};
    inline bool trace{}, installed{};
    inline thread_local bool extraLaunch{};
    inline std::function<bool()> usable;
    inline std::function<std::uint64_t()> epoch;
    inline std::function<void(RE::ArrowProjectile*)> prepare;
    using Launch = RE::ProjectileHandle* (*)(RE::ProjectileHandle*, RE::Projectile::LaunchData&);
    inline Launch original{};

    struct Shot {
        RE::NiPointer<RE::Projectile> first; // Retain native poison/enchantment ownership until the task completes.
        RE::TESAmmo* ammo{};
        RE::TESObjectWEAP* weapon{};
        RE::EnchantmentItem* enchantment{};
        RE::AlchemyItem* poison{};
        RE::NiPoint3 origin{};
        float pitch{}, yaw{};
        std::uint64_t generation{};
    };
    inline bool active(std::uint64_t generation) {
        return installed && usable && epoch && usable() && epoch() == generation;
    }
    inline std::int32_t count(RE::Actor* actor, RE::TESAmmo* ammo) {
        const auto inventory = actor->GetInventoryCounts([ammo](RE::TESBoundObject& item) { return &item == ammo; });
        const auto found = inventory.find(ammo);
        return found == inventory.end() ? 0 : std::max(0, found->second);
    }
    inline void fire(const Shot& shot)
    {
        if (!active(shot.generation)) return;
        const auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || !perk || !player->HasPerk(perk) || player->IsDead()) return;
        // The caller may finish setting launch damage AFTER Launch returns.
        // Read the retained original projectile after that transaction, never
        // the player's newly equipped weapon or an unfinished launch snapshot.
        if (!shot.first) return;
        const auto& finalized = shot.first->GetProjectileRuntimeData();
        const auto damage = finalized.weaponDamage, power = finalized.power, scale = finalized.scale;
        if (!std::isfinite(damage) || damage < 0 || !std::isfinite(power) || !std::isfinite(scale)) return;
        // This task runs after the original fire transaction has consumed its
        // ammunition. Only an actual spare of the SAME coating batch qualifies.
        const auto before = count(player, shot.ammo);
        if (before < 1) {
            if (trace) SKSE::log::info("Twin Shot: single bolt; no spare of ammo {:08X}", shot.ammo->GetFormID());
            return;
        }
        player->RemoveItem(shot.ammo, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        if (!active(shot.generation)) return; // Never write an old transaction into a newly loaded game.
        const auto after = count(player, shot.ammo);
        if (after != before - 1) {
            SKSE::log::warn("Twin Shot: extra cancelled; ammo {:08X} removal unconfirmed {} -> {}", shot.ammo->GetFormID(), before, after);
            return;
        }
        bool launched = false;
        try {
            // Construct through the pinned native wrapper, including its real
            // LaunchData vtable. Never memcpy an engine object or retain its
            // stack LaunchData/extra-data pointers across the deferred task.
            RE::Projectile::LaunchData data(player, shot.origin,
                RE::Projectile::ProjectileRot{shot.pitch, shot.yaw + spreadRadians}, shot.ammo, shot.weapon);
            data.poison = shot.poison;
            data.enchantItem = shot.enchantment;
            data.power = power;
            data.scale = scale;
            data.useOrigin = true;
            data.autoAim = false; // Preserve the original's resolved aim plus the small spread.
            struct ExtraGuard {
                bool previous = extraLaunch;
                ExtraGuard() { extraLaunch = true; }
                ~ExtraGuard() { extraLaunch = previous; }
            } guard;
            RE::ProjectileHandle handle;
            original(&handle, data);
            const auto second = handle.get();
            launched = static_cast<bool>(second);
            if (second) {
                // Preserve the first shot's actual launch damage, including
                // tempering. An equipment change before this task cannot
                // substitute another equipped weapon's damage or coating.
                auto& runtime = second->GetProjectileRuntimeData();
                runtime.weaponDamage = damage;
                if (const auto arrow = second->As<RE::ArrowProjectile>()) {
                    arrow->GetArrowRuntimeData().poison = shot.poison;
                    arrow->GetArrowRuntimeData().enchantItem = shot.enchantment;
                    if (prepare) prepare(arrow);
                }
                SKSE::log::info("Twin Shot: extra projectile={:08X} ammo={:08X} weapon={:08X} poison={:08X} damage={} spare-count {} -> {} spread=0.75deg",
                    second->GetFormID(), shot.ammo->GetFormID(), shot.weapon->GetFormID(),
                    shot.poison ? shot.poison->GetFormID() : 0, damage, before, after);
            }
        } catch (const std::exception& e) {
            SKSE::log::error("Twin Shot: extra launch failed: {}", e.what());
        }
        if (!launched && active(shot.generation)) {
            player->AddObjectToContainer(shot.ammo, nullptr, 1, nullptr);
            SKSE::log::warn("Twin Shot: extra launch failed; reserved bolt refunded");
        }
    }
    inline RE::ProjectileHandle* launch(RE::ProjectileHandle* result, RE::Projectile::LaunchData& data)
    {
        // Preserve the actual launch's identity before native code can modify
        // its arguments. Forward every original shot exactly once.
        const auto ammo = data.ammoSource;
        const auto weapon = data.weaponSource;
        const auto shooter = data.shooter;
        const bool duplicate = extraLaunch;
        const auto generation = epoch ? epoch() : 0;
        const auto returned = original(result, data);
        if (duplicate || !active(generation)) return returned;
        try {
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!player || shooter != player || !perk || !player->HasPerk(perk) ||
                !ammo || !ammo->IsBolt() || !weapon || !weapon->IsCrossbow() || data.spell) return returned;
            const auto first = returned ? returned->get() : RE::NiPointer<RE::Projectile>{};
            const auto arrow = first ? first->As<RE::ArrowProjectile>() : nullptr;
            const auto tasks = SKSE::GetTaskInterface();
            if (!arrow || !tasks) return returned;
            if (prepare) prepare(arrow);
            const auto& nativeArrow = arrow->GetArrowRuntimeData();
            Shot shot{first, ammo, weapon, nativeArrow.enchantItem, nativeArrow.poison,
                first->GetPosition(), first->GetAngleX(), first->GetAngleZ(), generation};
            if (!std::isfinite(shot.pitch) || !std::isfinite(shot.yaw)) return returned;
            tasks->AddTask([shot] {
                try { fire(shot); }
                catch (const std::exception& e) { SKSE::log::error("Twin Shot task failed: {}", e.what()); }
            });
        } catch (const std::exception& e) {
            SKSE::log::error("Twin Shot: could not queue extra bolt: {}", e.what());
        }
        return returned;
    }
    inline void install(RE::TESDataHandler* data, bool logging, std::function<bool()> allowed,
        std::function<std::uint64_t()> generation, std::function<void(RE::ArrowProjectile*)> coat)
    {
        trace = logging; usable = std::move(allowed); epoch = std::move(generation); prepare = std::move(coat);
        perk = data->LookupForm<RE::BGSPerk>(0x804, "CoatingMechanist.esp");
        if (!perk) { SKSE::log::warn("Twin Shot disabled: perk 804 missing; install CoatingMechanist.esp 0.5.0"); return; }
        // Exact signature and IDs from the pinned CommonLib Projectile.cpp.
        // MinHook relocates complete instructions; no guessed call-site offsets.
        REL::Relocation<std::uintptr_t> target{RELOCATION_ID(42928, 44108)};
        const auto init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
            SKSE::log::error("Twin Shot hook initialization failed: {}", static_cast<int>(init)); return;
        }
        const auto address = reinterpret_cast<void*>(target.address());
        const auto created = MH_CreateHook(address, reinterpret_cast<void*>(&launch), reinterpret_cast<void**>(&original));
        if (created != MH_OK) { SKSE::log::error("Twin Shot launch hook unavailable: {}", static_cast<int>(created)); return; }
        const auto enabled = MH_EnableHook(address);
        if (enabled != MH_OK) {
            MH_RemoveHook(address); original = nullptr;
            SKSE::log::error("Twin Shot launch hook could not be enabled: {}", static_cast<int>(enabled)); return;
        }
        installed = true;
        SKSE::log::info("Twin Shot ready: Marksman 80 + Alchemical Potency; 2 real bolts, 2 ammo, 0.75deg spread, deferred extra, no recursive duplication");
    }
}
