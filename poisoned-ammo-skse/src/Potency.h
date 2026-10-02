#pragma once
#include "PotencyCore.h"
#include <unordered_set>

namespace potency
{
    inline RE::BGSPerk* perk{};
    inline bool trace{};
    // Resolved from the winning item records at DataLoaded. No source records
    // are edited, and recipe proxies retain the original magic-effect pointers.
    inline std::unordered_set<RE::EffectSetting*> oilDamage;

    inline bool damageEffect(const RE::EffectSetting* effect)
    {
        if (!effect) return false;
        const auto& d = effect->data;
        using A = RE::EffectSetting::Archetype;
        using F = RE::EffectSetting::EffectSettingData::Flag;
        return (d.archetype == A::kValueModifier || d.archetype == A::kDualValueModifier) &&
            d.primaryAV == RE::ActorValue::kHealth && d.flags.any(F::kDetrimental) &&
            !d.flags.any(F::kNoMagnitude);
    }

    inline bool eligible(RE::ActiveEffect* effect, RE::Actor* caster, RE::MagicTarget* target)
    {
        if (!perk || !effect || !caster || caster != RE::PlayerCharacter::GetSingleton() ||
            !caster->HasPerk(perk) || !effect->spell || !effect->effect ||
            !damageEffect(effect->effect->baseEffect) || !target ||
            target->GetTargetAsActor() == caster) return false;
        const auto base = effect->effect->baseEffect;
        if (const auto poison = effect->spell->As<RE::AlchemyItem>())
            return poison->IsPoison() && oilDamage.contains(base);
        return false;
    }

    inline void apply(RE::ActiveEffect* effect, RE::Actor* caster, RE::MagicTarget* target)
    {
        if (!eligible(effect, caster, target)) return;
        const auto skill = caster->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAlchemy);
        const auto before = effect->magnitude;
        if (scaleDamage(effect->magnitude, skill) && trace)
            SKSE::log::info("Alchemical Potency: source={:08X}, effect={:08X}, Alchemy={}, multiplier={}, magnitude {} -> {}",
                effect->spell->GetFormID(), effect->effect->baseEffect->GetFormID(), skill,
                multiplier(skill), before, effect->magnitude);
    }

    inline void addItem(RE::TESDataHandler* data, const char* file, RE::FormID id)
    {
        auto item = data->LookupForm<RE::AlchemyItem>(id, file);
        if (!item) {
            SKSE::log::info("Alchemical Potency: optional item absent {}|{:06X}", file, id);
            return;
        }
        unsigned found = 0;
        for (const auto effect : item->effects) if (effect && damageEffect(effect->baseEffect)) {
            oilDamage.insert(effect->baseEffect); ++found;
        }
        SKSE::log::info("Alchemical Potency: {}|{:06X}, '{}' registered {} damage effects", file, id, item->GetName(), found);
        if (!found) SKSE::log::warn("Alchemical Potency: item {:08X} has no supported direct Health damage; inspect its winning effects", item->GetFormID());
    }

    inline void install(RE::TESDataHandler* data, bool logging)
    {
        trace = logging;
        oilDamage.clear();
        perk = data->LookupForm<RE::BGSPerk>(0x803, "CoatingMechanist.esp");
        if (!perk) { SKSE::log::info("Alchemical Potency disabled: perk 803 absent"); return; }
        for (RE::FormID id : {0x807u, 0x808u, 0x809u, 0x80Au}) addItem(data, "Requiem - Alchemy Redone.esp", id);
        for (RE::FormID id : {0xA6Du, 0xA6Eu, 0xA6Fu, 0xA70u}) addItem(data, "Big Tweaks.esp", id);
        SKSE::log::info("Alchemical Potency enabled: perk={:08X}, oil effects={}; coated crossbow bolts only; +1% oil damage per current Alchemy level",
            perk->GetFormID(), oilDamage.size());
    }
}
