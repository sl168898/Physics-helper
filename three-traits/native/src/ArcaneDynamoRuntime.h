#pragma once
#include "ArcaneDynamo.h"
#include <array>
#include <functional>

namespace traits {
class ArcaneDynamoRuntime {
    RE::SpellItem* trait{};
    std::function<bool()> inSession;
    bool installed{};
    std::uint32_t diagnostics{};
    static inline ArcaneDynamoRuntime* self{};

    struct Delivery {
        RE::MagicCaster* caster{};
        RE::MagicItem* enchantment{};
        RE::TESObjectWEAP* weapon{};
        bool paid{}, notified{};
    };
    struct Receipt { RE::MagicCaster* caster{}; RE::MagicItem* spell{}; bool valid{}; };
    static inline thread_local Delivery* delivery{};
    static inline thread_local RE::MagicItem* bypassCharge{};
    std::array<Receipt, 4> receipts{};

    using TargetsFn = bool (*)(RE::MagicCaster*, float, std::uint32_t&, RE::TESBoundObject*, bool, bool);
    using CheckFn = bool (*)(RE::ActorMagicCaster*, RE::MagicItem*, bool, float*, RE::MagicSystem::CannotCastReason*, bool);
    using CastFn = void (*)(RE::ActorMagicCaster*, bool, std::uint32_t, RE::MagicItem*);
    using ResourceFn = RE::ActorValue (*)(RE::MagicItem*, RE::MagicSystem::CastingSource);
    using AdjustFn = void (*)(RE::ActiveEffect*, float, bool);
    using ValueFn = float (*)(RE::ActorValueOwner*, RE::ActorValue);
    static inline TargetsFn originalTargets{};
    static inline REL::Relocation<CheckFn> originalCheck;
    static inline REL::Relocation<CastFn> originalCast;
    static inline REL::Relocation<ResourceFn> originalCheckResource, originalCastResource;
    static inline REL::Relocation<AdjustFn> originalAdjust;
    static inline REL::Relocation<ValueFn> originalValue;

    bool selected() const {
        auto p = RE::PlayerCharacter::GetSingleton();
        return installed && inSession && inSession() && p && trait && !p->IsDead() && p->HasSpell(trait);
    }
    static bool contact(RE::MagicItem* spell) {
        return spell && spell->As<RE::EnchantmentItem>() &&
            spell->GetSpellType() == RE::MagicSystem::SpellType::kEnchantment &&
            spell->GetCastingType() == RE::MagicSystem::CastingType::kFireAndForget &&
            spell->GetDelivery() == RE::MagicSystem::Delivery::kTouch;
    }
    static bool physicalWeapon(RE::TESObjectWEAP* weapon) {
        return weapon && !weapon->IsStaff() && !weapon->IsHandToHandMelee();
    }
    static RE::TESObjectWEAP* sourceWeapon(RE::MagicCaster* caster, RE::MagicItem* spell,
        RE::TESBoundObject* explicitSource = nullptr) {
        if (!caster || !contact(spell)) return nullptr;
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player || caster->GetCasterAsActor() != player) return nullptr;
        if (auto weapon = explicitSource ? explicitSource->As<RE::TESObjectWEAP>() : nullptr;
            physicalWeapon(weapon)) return weapon;
        const auto hand = caster->GetCastingSource();
        if (hand != RE::MagicSystem::CastingSource::kLeftHand &&
            hand != RE::MagicSystem::CastingSource::kRightHand) return nullptr;
        auto entry = player->GetEquippedEntryData(hand == RE::MagicSystem::CastingSource::kLeftHand);
        auto weapon = entry && entry->object ? entry->object->As<RE::TESObjectWEAP>() : nullptr;
        return physicalWeapon(weapon) && entry->GetEnchantment() == spell ? weapon : nullptr;
    }
    static float payment(RE::MagicItem* enchantment, RE::Actor* player) {
        if (RE::PlayerCharacter::IsGodMode()) return 0.f;
        // CalculateTotalGoldValue's documented/default null actor uses this same
        // CalculateCost routine. Both calls see the winning enchantment data.
        const float base = enchantment->CalculateMagickaCost(nullptr);
        const float final = enchantment->CalculateMagickaCost(player);
        return dynamo::cost(base, final);
    }
    Receipt* receipt(RE::MagicCaster* caster) {
        const auto hand = static_cast<std::uint32_t>(caster->GetCastingSource());
        return hand < receipts.size() ? &receipts[hand] : nullptr;
    }
    static RE::Actor* targetActor(RE::ActiveEffect* effect) {
        auto ref = effect && effect->target ? effect->target->GetTargetStatsObject() : nullptr;
        return ref ? ref->As<RE::Actor>() : nullptr;
    }
    void log(const char* result, RE::MagicItem* spell, float amount, std::uint32_t count) {
        if (diagnostics++ < 80)
            SKSE::log::info("[ArcaneDynamo] {} enchantment={:08X}; Magicka cost={}; targets={}",
                result, spell ? spell->GetFormID() : 0, amount, count);
    }
    static bool targets(RE::MagicCaster* caster, float power, std::uint32_t& count,
        RE::TESBoundObject* source, bool loading, bool onlyHostile) {
        auto s = self;
        auto spell = caster ? caster->currentSpell : nullptr;
        auto weapon = sourceWeapon(caster, spell, source);
        if (!s || !s->selected() || loading || !weapon)
            return originalTargets(caster, power, count, source, loading, onlyHostile);
        auto player = RE::PlayerCharacter::GetSingleton();
        const float amount = payment(spell, player);
        auto pending = s->receipt(caster);
        if (pending) *pending = {caster, spell, true};
        if (!dynamo::canPay(player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka), amount)) {
            count = 0;
            s->log("INSUFFICIENT MAGICKA", spell, amount, 0);
            return false;
        }
        // Reserve once BEFORE any effects can absorb Magicka. Multiple effects
        // on the same enchantment all run within this one native delivery.
        player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, -amount);
        Delivery current{caster, spell, weapon, true, false};
        dynamo::Scope<Delivery*> context(delivery, &current);
        const bool result = originalTargets(caster, power, count, source, loading, onlyHostile);
        if (count == 0) {
            player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, amount);
            s->log("NO TARGET / REFUNDED", spell, amount, count);
        } else s->log("PAID", spell, amount, count);
        // Some paths notify SpellCast inside FindTargets; others do so directly
        // afterward. Retain only an unconsumed receipt for the latter path.
        if (pending) *pending = {caster, spell, !current.notified};
        return result;
    }
    static bool check(RE::ActorMagicCaster* caster, RE::MagicItem* spell, bool dual,
        float* strength, RE::MagicSystem::CannotCastReason* reason, bool baseValue) {
        auto s = self;
        auto item = spell ? spell : (caster ? caster->currentSpell : nullptr);
        if (!s || !s->selected() || !sourceWeapon(caster, item))
            return originalCheck(caster, spell, dual, strength, reason, baseValue);
        // Preserve the game's other cast restrictions; replace only the resource
        // check. An empty soul-charge bar is allowed with sufficient Magicka.
        dynamo::Scope<RE::MagicItem*> scope(bypassCharge, item);
        const bool allowed = originalCheck(caster, spell, dual, strength, reason, baseValue);
        if (!allowed) return false;
        if (!dynamo::canPay(caster->actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka), payment(item, caster->actor))) {
            if (reason) *reason = RE::MagicSystem::CannotCastReason::kMagicka;
            return false;
        }
        return true;
    }
    static void cast(RE::ActorMagicCaster* caster, bool success, std::uint32_t count, RE::MagicItem* spell) {
        auto s = self;
        auto item = spell ? spell : (caster ? caster->currentSpell : nullptr);
        auto pending = s && caster ? s->receipt(caster) : nullptr;
        const bool current = delivery && delivery->caster == caster && delivery->enchantment == item;
        const bool received = pending && pending->valid && pending->caster == caster && pending->spell == item;
        if (!s || !s->selected() || !contact(item) || (!current && !received)) {
            originalCast(caster, success, count, spell);
            return;
        }
        if (current) delivery->notified = true;
        if (received) pending->valid = false;
        dynamo::Scope<RE::MagicItem*> scope(bypassCharge, item);
        // Keep events, visuals and other release behavior. The native charge
        // drain is bypassed only for this already managed delivery.
        originalCast(caster, success, count, spell);
    }
    static RE::ActorValue checkResource(RE::MagicItem* item, RE::MagicSystem::CastingSource hand) {
        return item && item == bypassCharge ? RE::ActorValue::kNone : originalCheckResource(item, hand);
    }
    static RE::ActorValue castResource(RE::MagicItem* item, RE::MagicSystem::CastingSource hand) {
        return item && item == bypassCharge ? RE::ActorValue::kNone : originalCastResource(item, hand);
    }
    static void adjust(RE::ActiveEffect* effect, float power, bool onlyHostile) {
        originalAdjust(effect, power, onlyHostile);
        auto context = delivery;
        if (!context || !context->paid || !effect || effect->spell != context->enchantment ||
            effect->flags.any(RE::ActiveEffect::Flag::kDispelled)) return;
        auto player = RE::PlayerCharacter::GetSingleton();
        auto caster = effect->caster.get();
        auto target = targetActor(effect);
        auto base = effect->GetBaseObject();
        if (!base || caster.get() != player || !target || target == player) return;
        using Flag = RE::EffectSetting::EffectSettingData::Flag;
        const bool damage = dynamo::damageEffect(static_cast<std::int32_t>(base->data.archetype),
            base->data.flags.any(Flag::kDetrimental), base->data.flags.any(Flag::kRecover),
            base->data.flags.any(Flag::kNoMagnitude), static_cast<std::int32_t>(base->data.primaryAV),
            static_cast<std::int32_t>(base->data.secondaryAV));
        const float before = effect->magnitude;
        effect->magnitude = dynamo::damage(before, damage);
        if (self && before != effect->magnitude && self->diagnostics++ < 80)
            SKSE::log::info("[ArcaneDynamo] DAMAGE effect={:08X}; magnitude={}->{}; duration={}",
                base->GetFormID(), before, effect->magnitude, effect->duration);
    }
    static float value(RE::ActorValueOwner* owner, RE::ActorValue av) {
        const float result = originalValue(owner, av);
        if (av != RE::ActorValue::kMagickaRate) return result;
        auto s = self;
        auto player = RE::PlayerCharacter::GetSingleton();
        return dynamo::regeneration(result, s && player && owner == player->AsActorValueOwner() && s->selected());
    }
public:
    void reset() { receipts = {}; diagnostics = 0; }
    bool init(RE::TESDataHandler* data, const char* plugin, std::function<bool()> session) {
        self = this; inSession = std::move(session);
        trait = data->LookupForm<RE::SpellItem>(0xF60, plugin);
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!trait || !player) { SKSE::log::error("Arcane Dynamo: missing trait/player; disabled"); return false; }
        // AE call sites from ProjectStaff c2d9d4266724c09316f14e84d9db8b8d82e1d9bb.
        // They call MagicUtilities::GetAssociatedResource for this exact caster.
        const auto checkAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33364,34145)}.address() + 0xBE;
        const auto castAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33362,34143)}.address() + 0x151;
        const auto adjustAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33763,34547)}.address() + 0x656;
        for (auto address : {checkAddress, castAddress, adjustAddress}) {
            if (*reinterpret_cast<const std::uint8_t*>(address) != 0xE8) {
                SKSE::log::error("Arcane Dynamo: resource/effect call is not E8; feature disabled"); return false;
            }
        }
        const auto targetAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33632,34410)}.address();
        const auto status = MH_CreateHook(reinterpret_cast<void*>(targetAddress), reinterpret_cast<void*>(targets),
            reinterpret_cast<void**>(&originalTargets));
        if (status != MH_OK) { SKSE::log::error("Arcane Dynamo: target hook creation failed {}", int(status)); return false; }
        if (const auto enabled = MH_EnableHook(reinterpret_cast<void*>(targetAddress)); enabled != MH_OK) {
            MH_RemoveHook(reinterpret_cast<void*>(targetAddress));
            SKSE::log::error("Arcane Dynamo: target hook enable failed {}", int(enabled)); return false;
        }
        originalCheckResource = SKSE::GetTrampoline().write_call<5>(checkAddress, checkResource);
        originalCastResource = SKSE::GetTrampoline().write_call<5>(castAddress, castResource);
        originalAdjust = SKSE::GetTrampoline().write_call<5>(adjustAddress, adjust);
        REL::Relocation<std::uintptr_t> casterTable{RE::VTABLE_ActorMagicCaster[0]};
        originalCheck = casterTable.write_vfunc(0xA, check);
        originalCast = casterTable.write_vfunc(0x9, cast);
        // Read the actual PlayerCharacter ActorValueOwner subobject's table,
        // avoiding a guessed multiple-inheritance vtable index.
        REL::Relocation<std::uintptr_t> valueTable{*reinterpret_cast<std::uintptr_t*>(player->AsActorValueOwner())};
        originalValue = valueTable.write_vfunc(0x1, value);
        installed = true;
        SKSE::log::info("Arcane Dynamo ready: damage x1.30; 15 Magicka normalized by native charge cost; regeneration x0.65; contact weapon enchantments only");
        return true;
    }
};
}
