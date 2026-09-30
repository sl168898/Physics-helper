// MIT; Physics-helper contributors, 2026-09-30.
// Classify loaded effects, never localized item names or the equipped weapon.
#pragma once
#include <RE/Skyrim.h>
#include "AmmoIconTypes.h"

namespace AmmoIcon
{
    inline Damage EffectType(RE::EffectSetting* effect, bool poisonSource)
    {
        if (!effect || (!effect->IsHostile() && !effect->IsDetrimental())) return Damage::None;
        const auto& data = effect->data;
        const bool health = data.primaryAV == RE::ActorValue::kHealth ||
            (data.archetype == RE::EffectArchetype::kDualValueModifier && data.secondaryAV == RE::ActorValue::kHealth);
        // Explicit damage keywords cover elemental effects with nonstandard
        // archetypes. Resistance/weakness effects have no such damage keyword.
        if (effect->HasKeywordString("MagicDamageFire")) return Damage::Fire;
        if (effect->HasKeywordString("MagicDamageFrost")) return Damage::Frost;
        if (effect->HasKeywordString("MagicDamageShock")) return Damage::Shock;
        if (effect->HasKeywordString("MagicDamagePoison")) return Damage::Poison;
        if (!health) return Damage::None;
        switch (data.resistVariable) {
        case RE::ActorValue::kResistFire: return Damage::Fire;
        case RE::ActorValue::kResistFrost: return Damage::Frost;
        case RE::ActorValue::kResistShock: return Damage::Shock;
        case RE::ActorValue::kPoisonResist: return Damage::Poison;
        default: return poisonSource ? Damage::Poison : Damage::None;
        }
    }
    inline Damage MagicType(RE::MagicItem* item, bool poisonSource = false)
    {
        Scores scores;
        if (item) for (auto effect : item->effects) {
            if (!effect || !effect->baseEffect) continue;
            const auto base = effect->baseEffect;
            using Flag = RE::EffectSetting::EffectSettingData::Flag;
            scores.Add(EffectType(base, poisonSource), effect->effectItem.magnitude, effect->effectItem.duration,
                base->data.flags.all(Flag::kNoMagnitude), base->data.flags.all(Flag::kNoDuration));
        }
        // Utility/status poisons keep the generic green coating indicator.
        return scores.Dominant(poisonSource ? Damage::Poison : Damage::None);
    }
    inline Damage NativeType(RE::TESAmmo* ammo)
    {
        if (!ammo) return Damage::None;
        const auto projectile = ammo->GetRuntimeData().data.projectile;
        if (!projectile || !projectile->data.flags.all(RE::BGSProjectileData::BGSProjectileFlags::kExplosion))
            return Damage::None;
        const auto explosion = projectile->data.explosionType;
        return explosion ? MagicType(explosion->formEnchanting) : Damage::None;
    }
}
