#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>

namespace traits
{
    // Only the retired trait is removed. The new tree perk is an independent
    // record, purchased and persisted by Skyrim's normal perk system.
    class VoiceAuthorityRuntime
    {
        RE::SpellItem* legacyAbility{};
        RE::BGSPerk* legacyPerk{};
        RE::BGSPerk* speechPerk{};
        RE::EffectSetting* legacyEffect{};
        RE::EffectSetting* legacyCooldown{};
        std::atomic_uint64_t generation = 0;

    public:
        void init(RE::TESDataHandler* data, const char* plugin)
        {
            if (!data) return;
            legacyAbility = data->LookupForm<RE::SpellItem>(0x800, plugin);
            legacyPerk = data->LookupForm<RE::BGSPerk>(0x802, plugin);
            speechPerk = data->LookupForm<RE::BGSPerk>(0x807, plugin);
            legacyEffect = data->LookupForm<RE::EffectSetting>(0x801, plugin);
            legacyCooldown = data->LookupForm<RE::EffectSetting>(0x806, plugin);
            if (!legacyAbility || !legacyPerk || !speechPerk || !legacyEffect || !legacyCooldown)
                SKSE::log::error("Voice of Authority: matching combined 2.11.0 forms are missing; legacy cleanup disabled");
            else
                SKSE::log::info("Voice of Authority: Speech 10 perk ready; old trait retired; tree placement uses Perk Adjuster");
        }

        void cancel() { ++generation; }

        void loaded(bool success)
        {
            const auto serial = ++generation;
            if (!success || !legacyAbility || !legacyPerk || !speechPerk || !legacyEffect || !legacyCooldown) return;
            SKSE::GetTaskInterface()->AddTask([this, serial] {
                if (generation.load() != serial) return;
                auto player = RE::PlayerCharacter::GetSingleton();
                auto target = player ? player->GetMagicTarget() : nullptr;
                if (!target) return;
                const bool hadAbility = player->HasSpell(legacyAbility);
                const bool hadPerk = player->HasPerk(legacyPerk);
                const bool hadEffects = target->HasMagicEffect(legacyEffect) || target->HasMagicEffect(legacyCooldown);
                if (!hadAbility && !hadPerk && !hadEffects) return;

                if (hadAbility) player->RemoveSpell(legacyAbility);
                // Also handle a stale saved active effect after the ability was
                // removed. Dispel the exact source spell; never edit recovery AVs.
                auto handle = player->GetHandle();
                target->DispelEffect(legacyAbility, handle);
                if (player->HasPerk(legacyPerk)) player->RemovePerk(legacyPerk);
                SKSE::log::info("Voice of Authority upgrade: legacy ability={}, perk={}, effects={}; "
                    "remaining ability={}, cooldown={}; buy the Speech perk normally",
                    hadAbility, hadPerk, hadEffects, player->HasSpell(legacyAbility),
                    target->HasMagicEffect(legacyCooldown));
            });
        }
    };
}
