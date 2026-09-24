#pragma once
#include "RunicOverdrive.h"
#include <functional>

namespace traits {
class RunicOverdriveRuntime {
    RE::SpellItem* trait = nullptr;
    std::function<bool()> inSession;
    bool installed = false;
    std::uint32_t diagnostics = 0;
    static inline RunicOverdriveRuntime* self = nullptr;
    static inline thread_local RunicHit hitting;
    using AdjustFn = void (*)(RE::ActiveEffect*, float, bool);
    static inline REL::Relocation<AdjustFn> originalAdjust;

    bool selected() const {
        auto p = RE::PlayerCharacter::GetSingleton();
        return installed && inSession && inSession() && p && !p->IsDead() && trait && p->HasSpell(trait);
    }
    static RE::Actor* targetActor(RE::ActiveEffect* effect) {
        auto ref = effect && effect->target ? effect->target->GetTargetStatsObject() : nullptr;
        return ref ? ref->As<RE::Actor>() : nullptr;
    }
    // Some engine paths apply a contact enchantment before ProcessHitData.
    // There is no timed buff: only the engine's currently active melee attack
    // and the exact weapon that supplied this enchantment may qualify there.
    static bool currentPowerAttack(RE::Actor* player, RE::TESObjectWEAP* source) {
        if (!source || !source->IsMelee() || source->IsHandToHandMelee()) return false;
        const auto state = player->AsActorState()->GetAttackState();
        if (state != RE::ATTACK_STATE_ENUM::kSwing && state != RE::ATTACK_STATE_ENUM::kHit &&
            state != RE::ATTACK_STATE_ENUM::kFollowThrough) return false;
        auto process = player->GetActorRuntimeData().currentProcess;
        auto attack = process && process->high ? process->high->attackData.get() : nullptr;
        if (!attack || !attack->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack) ||
            attack->data.flags.any(RE::AttackData::AttackFlag::kBashAttack)) return false;
        return player->GetEquippedObject(false) == source || player->GetEquippedObject(true) == source;
    }
    static void adjust(RE::ActiveEffect* effect, float power, bool onlyHostile) {
        originalAdjust(effect, power, onlyHostile);
        auto s = self;
        if (!s || !effect || !s->selected() || !effect->spell ||
            effect->flags.any(RE::ActiveEffect::Flag::kDispelled)) return;
        auto player = RE::PlayerCharacter::GetSingleton();
        auto caster = effect->caster.get();
        auto target = targetActor(effect);
        auto enchantment = effect->spell->As<RE::EnchantmentItem>();
        auto base = effect->GetBaseObject();
        if (caster.get() != player || !target || target == player || !enchantment || !base ||
            enchantment->GetDelivery() != RE::MagicSystem::Delivery::kTouch ||
            enchantment->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) return;
        auto source = effect->source ? effect->source->As<RE::TESObjectWEAP>() : nullptr;
        // A known physical hit is authoritative, even when it explicitly says
        // normal attack. A nested hit cannot inherit its caller's power flag.
        const bool eligible = hitting.present
            ? hitting.matches(player->GetFormID(), target->GetFormID(), source ? source->GetFormID() : 0)
            : currentPowerAttack(player, source);
        if (!eligible) return;
        using Flag = RE::EffectSetting::EffectSettingData::Flag;
        const auto mode = runicScaling(static_cast<std::int32_t>(base->data.archetype),
            base->data.flags.any(Flag::kNoMagnitude), base->data.flags.any(Flag::kNoDuration),
            effect->magnitude, effect->duration);
        const float oldMagnitude = effect->magnitude, oldDuration = effect->duration;
        if (mode == RunicScaling::magnitude) effect->magnitude *= 2.f;
        else if (mode == RunicScaling::duration) effect->duration *= 2.f;
        if (s->diagnostics++ < 64) {
            SKSE::log::info("[RunicOverdrive] {}: target={:08X}, enchantment={:08X}, effect={:08X}, source={:08X}, hit_context={}, magnitude={}->{}, duration={}->{}",
                mode == RunicScaling::none ? "UNSCALED" : "BOOST",
                target->GetFormID(), enchantment->GetFormID(), base->GetFormID(),
                source ? source->GetFormID() : 0, hitting.present,
                oldMagnitude, effect->magnitude, oldDuration, effect->duration);
        }
    }
public:
    void reset() { diagnostics = 0; hitting = {}; }
    RunicHitScope hit(RE::Actor* target, const RE::HitData& data) {
        const auto attacker = data.aggressor.get();
        auto player = RE::PlayerCharacter::GetSingleton();
        const bool power = data.flags.any(RE::HitData::Flag::kPowerAttack);
        const bool bash = data.flags.any(RE::HitData::Flag::kBash, RE::HitData::Flag::kTimedBash);
        const bool melee = data.weapon && data.weapon->IsMelee() && !data.weapon->IsHandToHandMelee();
        const bool eligible = runicEligible(selected(), attacker.get() == player, true, true, power, bash, melee) &&
                              !data.flags.any(RE::HitData::Flag::kPredictDamage);
        return RunicHitScope(hitting, {true, eligible,
            attacker ? attacker->GetFormID() : 0, target ? target->GetFormID() : 0,
            data.weapon ? data.weapon->GetFormID() : 0});
    }
    bool init(RE::TESDataHandler* data, const char* plugin, std::function<bool()> session) {
        static_assert(static_cast<int>(RE::EffectArchetypes::ArchetypeID::kParalysis) == 21);
        static_assert(static_cast<int>(RE::EffectArchetypes::ArchetypeID::kSoulTrap) == 23);
        static_assert(static_cast<int>(RE::EffectArchetypes::ArchetypeID::kPeakValueModifier) == 34);
        self = this; inSession = std::move(session);
        trait = data->LookupForm<RE::SpellItem>(0xF50, plugin);
        if (!trait) { SKSE::log::warn("Runic Overdrive: trait form absent; feature disabled"); return false; }
        // The same AE call site and three-argument ABI used by powerof3's
        // MagicSneakAttacks 37fe09931b9b1f1bd753da508132742d0f78aa33.
        const auto address = REL::Relocation<std::uintptr_t>{REL::RelocationID(33763, 34547)}.address() + 0x656;
        if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) {
            SKSE::log::error("Runic Overdrive: effect adjustment call is not E8; feature disabled"); return false;
        }
        originalAdjust = SKSE::GetTrampoline().write_call<5>(address, adjust);
        installed = true;
        SKSE::log::info("Runic Overdrive ready: hit enchantment magnitude x2; duration-only effects x2; native power attack Stamina x1.30; scripted procs preserved");
        return true;
    }
};
}
