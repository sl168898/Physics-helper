#pragma once
#include "ArcaneDynamo.h"
#include <array>
#include <functional>
#include <intrin.h>

namespace traits {
class ArcaneDynamoRuntime {
    RE::SpellItem* trait{};
    std::function<bool()> inSession;
    bool installed{};
    std::uint32_t diagnostics{};
    std::uint32_t chargeDiagnostics{};
    static inline ArcaneDynamoRuntime* self{};

    struct Delivery {
        RE::MagicCaster* caster{};
        RE::MagicItem* enchantment{};
        RE::TESObjectWEAP* weapon{};
        bool paid{};
    };
    static inline thread_local Delivery* delivery{};
    static inline thread_local const dynamo::CostContext* costContext{};
    static inline thread_local bool readingOriginalCost{};

    using TargetsFn = bool (*)(RE::MagicCaster*, float, std::uint32_t&, RE::TESBoundObject*, bool, bool);
    using CheckFn = bool (*)(RE::ActorMagicCaster*, RE::MagicItem*, bool, float*, RE::MagicSystem::CannotCastReason*, bool);
    using CastFn = void (*)(RE::ActorMagicCaster*, bool, std::uint32_t, RE::MagicItem*);
    using CostFn = float (*)(const RE::MagicItem*, RE::Actor*);
    using AdjustFn = void (*)(RE::ActiveEffect*, float, bool);
    using ValueFn = float (*)(RE::ActorValueOwner*, RE::ActorValue);
    static inline TargetsFn originalTargets{};
    static inline CostFn originalCost{};
    static inline CheckFn originalNativeCheck{};
    static inline CastFn originalNativeCast{};
    static inline REL::Relocation<CheckFn> originalCheck;
    static inline REL::Relocation<CastFn> originalCast;
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
        const auto equipped = [player, spell](bool left) -> RE::TESObjectWEAP* {
            auto entry = player->GetEquippedEntryData(left);
            auto weapon = entry && entry->object ? entry->object->As<RE::TESObjectWEAP>() : nullptr;
            return physicalWeapon(weapon) && entry->GetEnchantment() == spell ? weapon : nullptr;
        };
        if (hand == RE::MagicSystem::CastingSource::kLeftHand) return equipped(true);
        if (hand == RE::MagicSystem::CastingSource::kRightHand) return equipped(false);
        // Instant/other casters have no useful hand index. Require an exact
        // equipped enchantment match, rather than rejecting their charge checks.
        if (auto weapon = equipped(false)) return weapon;
        return equipped(true);
    }
    static float payment(RE::MagicItem* enchantment, RE::Actor* player) {
        if (RE::PlayerCharacter::IsGodMode()) return 0.f;
        dynamo::Scope<bool> probe(readingOriginalCost, true);
        // CalculateTotalGoldValue's documented/default null actor uses this same
        // CalculateCost routine. Both calls see the winning enchantment data.
        const float base = enchantment->CalculateMagickaCost(nullptr);
        const float final = enchantment->CalculateMagickaCost(player);
        return dynamo::cost(base, final);
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
        if (!dynamo::canPay(player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka), amount)) {
            count = 0;
            s->log("INSUFFICIENT MAGICKA", spell, amount, 0);
            return false;
        }
        // Reserve once BEFORE any effects can absorb Magicka. Multiple effects
        // on the same enchantment all run within this one native delivery.
        player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, -amount);
        Delivery current{caster, spell, weapon, true};
        dynamo::Scope<Delivery*> context(delivery, &current);
        const dynamo::CostContext charge{player, spell};
        dynamo::Scope<const dynamo::CostContext*> chargeScope(costContext, &charge);
        const bool result = originalTargets(caster, power, count, source, loading, onlyHostile);
        if (count == 0) {
            player->AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kMagicka, amount);
            s->log("NO TARGET / REFUNDED", spell, amount, count);
        } else s->log("PAID", spell, amount, count);
        return result;
    }
    static bool checkImpl(CheckFn next, RE::ActorMagicCaster* caster, RE::MagicItem* spell, bool dual,
        float* strength, RE::MagicSystem::CannotCastReason* reason, bool baseValue) {
        auto s = self;
        auto item = spell ? spell : (caster ? caster->currentSpell : nullptr);
        if (!s || !s->selected() || !sourceWeapon(caster, item))
            return next(caster, spell, dual, strength, reason, baseValue);
        if (costContext && costContext->matches(caster->actor, item))
            return next(caster, spell, dual, strength, reason, baseValue);
        const dynamo::CostContext charge{caster->actor, item};
        dynamo::Scope<const dynamo::CostContext*> scope(costContext, &charge);
        const bool allowed = next(caster, spell, dual, strength, reason, baseValue);
        if (s->chargeDiagnostics++ < 120)
            SKSE::log::info("[ArcaneDynamo] CHECK enchantment={:08X}; casting_source={}; native_allowed={}",
                item->GetFormID(), int(caster->GetCastingSource()), allowed);
        if (!allowed) return false;
        if (!dynamo::canPay(caster->actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka), payment(item, caster->actor))) {
            if (reason) *reason = RE::MagicSystem::CannotCastReason::kMagicka;
            return false;
        }
        return true;
    }
    static bool check(RE::ActorMagicCaster* caster, RE::MagicItem* spell, bool dual,
        float* strength, RE::MagicSystem::CannotCastReason* reason, bool baseValue) {
        return checkImpl(originalCheck.get(), caster, spell, dual, strength, reason, baseValue);
    }
    static bool nativeCheck(RE::ActorMagicCaster* caster, RE::MagicItem* spell, bool dual,
        float* strength, RE::MagicSystem::CannotCastReason* reason, bool baseValue) {
        return checkImpl(originalNativeCheck, caster, spell, dual, strength, reason, baseValue);
    }
    static void castImpl(CastFn next, RE::ActorMagicCaster* caster, bool success, std::uint32_t count, RE::MagicItem* spell) {
        auto s = self;
        auto item = spell ? spell : (caster ? caster->currentSpell : nullptr);
        if (!s || !s->selected() || !sourceWeapon(caster, item) ||
            (costContext && costContext->matches(caster->actor, item))) {
            next(caster, success, count, spell);
            return;
        }
        // No receipt/timing dependency: validation, release and delivery all
        // suppress charge for the same eligible cast, in any native order.
        const dynamo::CostContext charge{caster->actor, item};
        dynamo::Scope<const dynamo::CostContext*> scope(costContext, &charge);
        auto av = caster->actor->AsActorValueOwner();
        const float right = av->GetActorValue(RE::ActorValue::kRightItemCharge);
        const float left = av->GetActorValue(RE::ActorValue::kLeftItemCharge);
        next(caster, success, count, spell);
        if (s->chargeDiagnostics++ < 120)
            SKSE::log::info("[ArcaneDynamo] RELEASE enchantment={:08X}; casting_source={}; success={}; targets={}; charge_right={}->{}; charge_left={}->{}",
                item->GetFormID(), int(caster->GetCastingSource()), success, count, right,
                av->GetActorValue(RE::ActorValue::kRightItemCharge), left,
                av->GetActorValue(RE::ActorValue::kLeftItemCharge));
    }
    static void cast(RE::ActorMagicCaster* caster, bool success, std::uint32_t count, RE::MagicItem* spell) {
        castImpl(originalCast.get(), caster, success, count, spell);
    }
    static void nativeCast(RE::ActorMagicCaster* caster, bool success, std::uint32_t count, RE::MagicItem* spell) {
        castImpl(originalNativeCast, caster, success, count, spell);
    }
    static float calculateCost(const RE::MagicItem* item, RE::Actor* actor) {
        const float raw = originalCost(item, actor);
        const float result = dynamo::nativeChargeCost(raw, costContext, actor, item, readingOriginalCost);
        if (!readingOriginalCost && self && self->selected() && actor == RE::PlayerCharacter::GetSingleton() &&
            contact(const_cast<RE::MagicItem*>(item)) && self->chargeDiagnostics++ < 120)
            SKSE::log::info("[ArcaneDynamo] {} enchantment={:08X}; native_cost={}->{}; caller={:X}",
                costContext && costContext->matches(actor, item) ? "CHARGE COST SUPPRESSED" : "UNSCOPED COST FORWARDED",
                item->GetFormID(), raw, result, reinterpret_cast<std::uintptr_t>(_ReturnAddress()));
        return result;
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
    void reset() { diagnostics = 0; chargeDiagnostics = 0; }
    bool init(RE::TESDataHandler* data, const char* plugin, std::function<bool()> session) {
        self = this; inSession = std::move(session);
        trait = data->LookupForm<RE::SpellItem>(0xF60, plugin);
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!trait || !player) { SKSE::log::error("Arcane Dynamo: missing trait/player; disabled"); return false; }
        const auto adjustAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33763,34547)}.address() + 0x656;
        if (*reinterpret_cast<const std::uint8_t*>(adjustAddress) != 0xE8) {
            SKSE::log::error("Arcane Dynamo: effect call is not E8; feature disabled"); return false;
        }
        struct Hook { std::uintptr_t address; void* replacement; void** original; };
        const Hook hooks[] = {
            {REL::Relocation<std::uintptr_t>{REL::RelocationID(33632,34410)}.address(), reinterpret_cast<void*>(targets), reinterpret_cast<void**>(&originalTargets)},
            {REL::Relocation<std::uintptr_t>{RE::Offset::MagicItem::CalculateCost}.address(), reinterpret_cast<void*>(calculateCost), reinterpret_cast<void**>(&originalCost)},
            {REL::Relocation<std::uintptr_t>{REL::RelocationID(33364,34145)}.address(), reinterpret_cast<void*>(nativeCheck), reinterpret_cast<void**>(&originalNativeCheck)},
            {REL::Relocation<std::uintptr_t>{REL::RelocationID(33362,34143)}.address(), reinterpret_cast<void*>(nativeCast), reinterpret_cast<void**>(&originalNativeCast)}
        };
        std::array<void*, 4> created{};
        std::size_t createdCount{};
        for (const auto& hook : hooks) {
            auto address = reinterpret_cast<void*>(hook.address);
            auto status = MH_CreateHook(address, hook.replacement, hook.original);
            if (status == MH_OK) { created[createdCount++] = address; status = MH_EnableHook(address); }
            if (status != MH_OK) {
                for (std::size_t i = 0; i < createdCount; ++i) { MH_DisableHook(created[i]); MH_RemoveHook(created[i]); }
                SKSE::log::error("Arcane Dynamo: native hook installation failed {}; feature disabled", int(status)); return false;
            }
        }
        originalAdjust = SKSE::GetTrampoline().write_call<5>(adjustAddress, adjust);
        REL::Relocation<std::uintptr_t> casterTable{RE::VTABLE_ActorMagicCaster[0]};
        originalCheck = casterTable.write_vfunc(0xA, check);
        originalCast = casterTable.write_vfunc(0x9, cast);
        // Read the actual PlayerCharacter ActorValueOwner subobject's table,
        // avoiding a guessed multiple-inheritance vtable index.
        REL::Relocation<std::uintptr_t> valueTable{*reinterpret_cast<std::uintptr_t*>(player->AsActorValueOwner())};
        originalValue = valueTable.write_vfunc(0x1, value);
        installed = true;
        SKSE::log::info("Arcane Dynamo 1.6.1 ready: scoped native charge cost=0; damage x1.30; 15 Magicka normalized by original charge cost; regeneration x0.65; direct and virtual cast paths covered");
        return true;
    }
};
}
