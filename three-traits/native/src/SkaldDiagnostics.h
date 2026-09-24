#pragma once
#include <cmath>

namespace traits {
// Read-only, bounded probes of the actual winning spell/effects. In particular,
// instant restoration may never remain in the Active Effects list, so also
// report actor values before, immediately after, and after a short game update.
class SkaldDiagnostics {
    unsigned budget = 200;
    RE::SpellItem* pending = nullptr;
    float delay = 0;
    bool take() { if (!budget) return false; --budget; return true; }
    void snapshot(RE::PlayerCharacter* p, RE::SpellItem* spell, const char* phase) {
        if (!p || !spell || !take()) return;
        auto av = p->AsActorValueOwner();
        using AV = RE::ActorValue;
        auto maximum = [p, av](AV value) { return av->GetPermanentActorValue(value) +
            p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, value); };
        SKSE::log::info("[SkaldBuff] {} spell={:08X}: Health={}/{}, Stamina={}/{}, Magicka={}/{}; observations include other game effects",
            phase, spell->GetFormID(), av->GetActorValue(AV::kHealth), maximum(AV::kHealth),
            av->GetActorValue(AV::kStamina), maximum(AV::kStamina),
            av->GetActorValue(AV::kMagicka), maximum(AV::kMagicka));
        auto target = p->GetMagicTarget();
        auto effects = target ? target->GetActiveEffectList() : nullptr;
        unsigned count = 0;
        if (effects) for (auto effect : *effects) {
            if (!effect || effect->spell != spell) continue;
            ++count;
            auto base = effect->GetBaseObject();
            if (take()) SKSE::log::info("[SkaldBuff] {} ACTIVE: effect={:08X}, magnitude={}, duration={}, elapsed={}, inactive={}, dispelled={}",
                phase, base ? base->GetFormID() : 0, effect->magnitude, effect->duration, effect->elapsedSeconds,
                effect->flags.any(RE::ActiveEffect::Flag::kInactive), effect->flags.any(RE::ActiveEffect::Flag::kDispelled));
        }
        if (take()) SKSE::log::info("[SkaldBuff] {}: {} matching effect instances (not proof of successful stat restoration)", phase, count);
    }
public:
    void reset() { budget = 200; pending = nullptr; delay = 0; }
    void cancel() { pending = nullptr; delay = 0; }
    void before(RE::PlayerCharacter* p, RE::TESShout* shout, RE::SpellItem* spell) {
        if (!budget || !p || !shout || !spell || spell->GetDelivery() != RE::MagicSystem::Delivery::kSelf) return;
        if (take()) SKSE::log::info("[SkaldBuff] BEGIN: shout={} ({:08X}), first-word spell={:08X}, casting={}, delivery={}, effects={}; normal cast mode, magnitude override 0 (retain originals)",
            shout->GetName(), shout->GetFormID(), spell->GetFormID(), static_cast<int>(spell->GetCastingType()),
            static_cast<int>(spell->GetDelivery()), spell->effects.size());
        for (auto effect : spell->effects) {
            if (!effect || !effect->baseEffect || !take()) continue;
            auto base = effect->baseEffect;
            SKSE::log::info("[SkaldBuff] SOURCE: effect={} ({:08X}), magnitude={}, duration={}, area={}, archetype={}, primaryAV={}, hostile={}",
                base->GetName(), base->GetFormID(), effect->effectItem.magnitude, effect->effectItem.duration,
                effect->effectItem.area, static_cast<int>(base->data.archetype), static_cast<int>(base->data.primaryAV), base->IsHostile());
        }
        snapshot(p, spell, "BEFORE");
        pending = spell; delay = 0.25f;
    }
    void after(RE::PlayerCharacter* p, RE::SpellItem* spell) {
        if (pending == spell) snapshot(p, spell, "IMMEDIATE");
    }
    void tick(RE::PlayerCharacter* p, float dt, bool enabled) {
        if (!enabled || !p || p->IsDead()) { cancel(); return; }
        if (!pending || !std::isfinite(dt) || dt <= 0) return;
        delay -= dt;
        if (delay > 0) return;
        auto spell = pending; cancel(); snapshot(p, spell, "AFTER_UPDATE");
    }
};
}
