#pragma once
#include "IronLungs.h"
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace traits {
// Skyrim 1.6.1170 only, like the containing helper. The release handler is
// the transaction boundary. Instant casts (including Skald) never enter it.
// Projectile handles carry each transaction to the actual engine hit path.
class IronLungsRuntime {
    using Cast = std::shared_ptr<IronLungsCast>;
    struct Shot { RE::ProjectileHandle handle; Cast cast; RE::MagicItem* spell; };
    struct Scope {
        Cast& slot; Cast previous;
        Scope(Cast& s, Cast value) : slot(s), previous(s) { slot = std::move(value); }
        ~Scope() { slot = std::move(previous); }
    };
    inline static IronLungsRuntime* self = nullptr;
    inline static thread_local Cast releasing, launching, hitting;
    inline static thread_local bool applyingBonus = false;
    inline static REL::Relocation<bool (*)(RE::ActorMagicCaster*, RE::MagicItem*, bool, float*, RE::MagicSystem::CannotCastReason*, bool)> originalCheck;
    inline static REL::Relocation<bool (*)(RE::VoiceSpellFireHandler*, RE::Actor&, const RE::BSFixedString&)> originalVoice;
    using LaunchFn = RE::ProjectileHandle* (*)(RE::ProjectileHandle*, RE::Projectile::LaunchData&);
    using FindFn = bool (*)(RE::MagicCaster*, float, std::uint32_t&, RE::TESBoundObject*, bool, bool);
    using AddFn = bool (*)(RE::MagicTarget*, RE::MagicTarget::AddTargetData&);
    inline static LaunchFn originalLaunch = nullptr;
    inline static FindFn originalFind = nullptr;
    inline static AddFn originalAdd = nullptr;
    std::recursive_mutex mutex;
    std::unordered_map<std::uint32_t, Shot> shots;
    RE::SpellItem *trait = nullptr, *bonusSpell = nullptr;
    RE::TESShout* unrelenting = nullptr;
    std::function<bool()> inSession, skaldCasting;
    std::uint64_t serial = 0;
    float pruneAfter = 0;
    bool installed = false, warnedUnavailable = false;
    unsigned logBudget = 200;
    std::chrono::steady_clock::time_point lastFailure{};

    bool logNext() { if (!logBudget) return false; --logBudget; return true; }
    bool selected() const {
        auto p = RE::PlayerCharacter::GetSingleton();
        return installed && inSession && inSession() && p && trait && p->HasSpell(trait);
    }
    bool isForce(RE::MagicItem* spell) const {
        if (!spell || !unrelenting) return false;
        for (const auto& word : unrelenting->variations) if (word.spell == spell) return true;
        return false;
    }
    IronLungsQuote quote(RE::Actor* p) const {
        auto av = p->AsActorValueOwner();
        const float current = av->GetActorValue(RE::ActorValue::kStamina);
        // Permanent value contains base plus permanent modifiers; temporary
        // modifiers include Fortify Stamina equipment, food and other buffs.
        const float maximum = av->GetPermanentActorValue(RE::ActorValue::kStamina) +
            p->GetActorValueModifier(RE::ACTOR_VALUE_MODIFIER::kTemporary, RE::ActorValue::kStamina);
        return quoteIronLungs(current, maximum, av->GetActorValue(RE::ActorValue::kShoutRecoveryMult));
    }
    void failure(const IronLungsQuote& q) {
        const auto now = std::chrono::steady_clock::now();
        if (now - lastFailure < std::chrono::milliseconds(750)) return;
        lastFailure = now;
        RE::DebugNotification("Iron Lungs: not enough Stamina.");
        if (logNext()) SKSE::log::info("[IronLungs] BLOCKED: stamina={}, cost={}", q.staminaBefore, q.cost);
    }
    static bool check(RE::ActorMagicCaster* caster, RE::MagicItem* spell, bool dual, float* strength,
                      RE::MagicSystem::CannotCastReason* reason, bool baseCost) {
        if (!originalCheck(caster, spell, dual, strength, reason, baseCost)) return false;
        auto s = self;
        if (!s || !caster || !s->selected() || caster->actor != RE::PlayerCharacter::GetSingleton() ||
            caster->castingSource != RE::MagicSystem::CastingSource::kOther || !s->isForce(spell)) return true;
        std::lock_guard lock(s->mutex);
        // A successful release has already reserved its one Stamina payment.
        if (releasing) return true;
        const auto q = s->quote(caster->actor);
        if (q.allowed) return true;
        // No Magicka error: our own message identifies the actual resource.
        if (reason) *reason = RE::MagicSystem::CannotCastReason::kOK;
        s->failure(q);
        return false;
    }
    static bool voice(RE::VoiceSpellFireHandler* handler, RE::Actor& actor, const RE::BSFixedString& tag) {
        auto s = self;
        if (!s || !s->selected() || &actor != RE::PlayerCharacter::GetSingleton() ||
            actor.GetCurrentShout() != s->unrelenting || (s->skaldCasting && s->skaldCasting()))
            return originalVoice(handler, actor, tag);
        std::lock_guard lock(s->mutex);
        const auto q = s->quote(&actor);
        // Check again at release, since sprinting/attacks can spend Stamina
        // while the player is holding the shout key for more words.
        if (!q.allowed) {
            s->failure(q);
            if (auto caster = actor.GetMagicCaster(RE::MagicSystem::CastingSource::kOther)) caster->InterruptCast(false);
            return false; // no vanilla release, projectile, knockback or VoiceFire event
        }
        auto cast = std::make_shared<IronLungsCast>();
        cast->quote = q; cast->serial = ++s->serial;
        Scope releaseScope(releasing, cast);
        const float priorRecovery = std::max(0.f, actor.GetVoiceRecoveryTime());
        actor.AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, -q.cost);
        const bool result = originalVoice(handler, actor, tag);
        auto process = actor.GetActorRuntimeData().currentProcess;
        if (cast->projectiles) {
            // Only undo the recovery produced by THIS synchronous UF release.
            // Do not reset the timer later, or alter any other shout's records.
            if (process && process->high) process->high->voiceRecoveryTime = priorRecovery;
            if (s->logNext()) SKSE::log::info("[IronLungs] CAST {}: stamina={}, cost={}, base_magic_bonus={}, projectiles={}, release_result={}",
                cast->serial, q.staminaBefore, q.cost, q.bonus, cast->projectiles, result);
        } else {
            actor.AsActorValueOwner()->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, RE::ActorValue::kStamina, q.cost);
            if (s->logNext()) SKSE::log::warn("[IronLungs] CAST {}: no UF projectile launched; Stamina refunded; release_result={}", cast->serial, result);
        }
        return result;
    }
    static RE::ProjectileHandle* launch(RE::ProjectileHandle* result, RE::Projectile::LaunchData& data) {
        auto s = self;
        const bool tag = s && releasing && data.shooter == RE::PlayerCharacter::GetSingleton() &&
            data.castingSource == RE::MagicSystem::CastingSource::kOther && s->isForce(data.spell);
        // Some projectiles can find targets during their initialization.
        Scope launchScope(launching, tag ? releasing : Cast{});
        auto answer = originalLaunch(result, data);
        if (tag && answer && answer->get()) {
            std::lock_guard lock(s->mutex);
            s->shots.insert_or_assign(answer->native_handle(), Shot{*answer, releasing, data.spell});
            ++releasing->projectiles;
        }
        return answer;
    }
    static bool find(RE::MagicCaster* caster, float effectiveness, std::uint32_t& count,
                     RE::TESBoundObject* source, bool loadCast, bool hostileOnly) {
        auto s = self;
        Cast context;
        if (s && caster && !applyingBonus && s->isForce(caster->currentSpell)) {
            std::lock_guard lock(s->mutex);
            RE::Actor* blame = nullptr;
            auto ref = caster->GetCasterObjectReference(&blame);
            auto projectile = ref ? ref->AsProjectile() : nullptr;
            if (projectile) {
                const auto it = s->shots.find(projectile->GetHandle().native_handle());
                if (it != s->shots.end() && it->second.spell == caster->currentSpell) context = it->second.cast;
            }
            if (!context && launching) context = launching;
        }
        Scope hitScope(hitting, std::move(context));
        return originalFind(caster, effectiveness, count, source, loadCast, hostileOnly);
    }
    void damage(RE::Actor* target, RE::MagicItem* originalSpell, const Cast& cast) {
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!target || !player || target == player || target->IsDead() || !bonusSpell) return;
        std::lock_guard lock(mutex);
        if (!cast->claimTarget(target->GetFormID())) return;
        float magnitude = cast->quote.bonus;
        using Entry = RE::BGSEntryPoint::ENTRY_POINT;
        // Evaluate the actual UF spell, including its keywords and exact ID.
        // The helper effect has Power Affects Magnitude OFF, so these native
        // outgoing/incoming magnitude modifiers are applied exactly once.
        RE::BGSEntryPoint::HandleEntryPoint(Entry::kModSpellMagnitude, player, originalSpell, target, &magnitude);
        RE::BGSEntryPoint::HandleEntryPoint(Entry::kModIncomingSpellMagnitude, target, originalSpell, player, &magnitude);
        if (!std::isfinite(magnitude) || magnitude <= 0) return;
        auto caster = player->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) return;
        struct BonusScope { bool previous = applyingBonus; BonusScope() { applyingBonus = true; } ~BonusScope() { applyingBonus = previous; } } scope;
        // A real, non-elemental Health-damage magic effect. Native magic
        // resistance/absorption and damage processing remain responsible for
        // the final health loss. Never use direct DamageActorValue(Health).
        caster->CastSpellImmediate(bonusSpell, true, target, 1.f, false, magnitude, player);
        if (logNext()) SKSE::log::info("[IronLungs] HIT {}: target={:08X}, source_spell={:08X}, magic_before_resistance={}",
            cast->serial, target->GetFormID(), originalSpell->GetFormID(), magnitude);
    }
    static bool add(RE::MagicTarget* target, RE::MagicTarget::AddTargetData& data) {
        auto s = self;
        const Cast context = hitting;
        const bool eligible = s && context && !applyingBonus && s->isForce(data.magicItem);
        const bool accepted = originalAdd(target, data);
        if (accepted && eligible && target) {
            // GetTargetAsActor in the pinned library does not adjust the AE
            // secondary-base pointer. Obtain the real reference virtually.
            auto ref = target->GetTargetStatsObject();
            auto actor = ref ? ref->As<RE::Actor>() : nullptr;
            s->damage(actor, data.magicItem, context);
        }
        return accepted;
    }
public:
    void reset() {
        std::lock_guard lock(mutex);
        shots.clear(); pruneAfter = 0; logBudget = 200; warnedUnavailable = false;
        lastFailure = {};
    }
    void tick(float dt) {
        std::lock_guard lock(mutex);
        auto p = RE::PlayerCharacter::GetSingleton();
        if (!installed && !warnedUnavailable && inSession && inSession() && p && trait && p->HasSpell(trait)) {
            warnedUnavailable = true;
            RE::DebugNotification("Iron Lungs unavailable: check BiggieTraitMechanics.log.");
        }
        if (!std::isfinite(dt) || dt <= 0) return;
        pruneAfter -= dt;
        if (pruneAfter > 0) return;
        pruneAfter = 1.f;
        std::erase_if(shots, [](const auto& item) { auto p = item.second.handle.get(); return !p || p->IsDeleted(); });
    }
    bool init(RE::TESDataHandler* data, const char* plugin, std::function<bool()> session, std::function<bool()> isSkald) {
        self = this; inSession = std::move(session); skaldCasting = std::move(isSkald);
        trait = data->LookupForm<RE::SpellItem>(0xF30, plugin);
        bonusSpell = data->LookupForm<RE::SpellItem>(0xF32, plugin);
        unrelenting = RE::TESForm::LookupByID<RE::TESShout>(0x13E07);
        if (!trait || !bonusSpell || !unrelenting || bonusSpell->effects.size() != 1) {
            SKSE::log::error("Iron Lungs: required forms missing; trait disabled"); return false;
        }
        for (const auto& word : unrelenting->variations) {
            if (!word.spell || word.spell->GetDelivery() != RE::MagicSystem::Delivery::kAimed ||
                word.spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) {
                SKSE::log::error("Iron Lungs: unsupported UF delivery/casting override; trait disabled"); return false;
            }
        }
        // All function signatures and IDs come from the pinned CommonLib.
        // AddTarget AE 34526 is independently used by TiltedEvolution.
        const auto launchAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(42928, 44108)}.address();
        const auto findAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(33632, 34410)}.address();
        const auto addAddress = REL::Relocation<std::uintptr_t>{REL::ID(34526)}.address();
        struct Hook { std::uintptr_t address; void* replacement; void** original; };
        const Hook hooks[] = {
            {launchAddress, reinterpret_cast<void*>(launch), reinterpret_cast<void**>(&originalLaunch)},
            {findAddress, reinterpret_cast<void*>(find), reinterpret_cast<void**>(&originalFind)},
            {addAddress, reinterpret_cast<void*>(add), reinterpret_cast<void**>(&originalAdd)}
        };
        std::vector<void*> created;
        for (const auto& h : hooks) {
            auto address = reinterpret_cast<void*>(h.address);
            auto status = MH_CreateHook(address, h.replacement, h.original);
            if (status == MH_OK) { created.push_back(address); status = MH_EnableHook(address); }
            if (status != MH_OK) {
                for (auto old : created) { MH_DisableHook(old); MH_RemoveHook(old); }
                SKSE::log::error("Iron Lungs: hook installation failed {}; trait disabled", int(status)); return false;
            }
        }
        REL::Relocation<std::uintptr_t> casterTable{RE::VTABLE_ActorMagicCaster[0]};
        originalCheck = casterTable.write_vfunc(0xA, check);
        REL::Relocation<std::uintptr_t> voiceTable{RE::VTABLE_VoiceSpellFireHandler[0]};
        originalVoice = voiceTable.write_vfunc(0x1, voice);
        installed = true;
        SKSE::log::info("Iron Lungs ready: magic bonus; strict Stamina gate; 25% maximum cost, 10% floor; 25% pre-payment current Stamina damage; Skald exempt");
        return true;
    }
};
}
