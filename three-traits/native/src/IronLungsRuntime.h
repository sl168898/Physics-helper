#pragma once
#include "IronLungs.h"
#include "IronLungsGrant.h"
#include "IronLungsImpact.h"
#include "Overexertion.h"
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
    inline static thread_local Cast releasing, launching;
    inline static thread_local RE::MagicItem* launchingSpell = nullptr;
    inline static thread_local IronLungsImpact hitting;
    inline static thread_local bool applyingBonus = false;
    inline static REL::Relocation<bool (*)(RE::ActorMagicCaster*, RE::MagicItem*, bool, float*, RE::MagicSystem::CannotCastReason*, bool)> originalCheck;
    inline static REL::Relocation<bool (*)(RE::VoiceSpellFireHandler*, RE::Actor&, const RE::BSFixedString&)> originalVoice;
    using LaunchFn = RE::ProjectileHandle* (*)(RE::ProjectileHandle*, RE::Projectile::LaunchData&);
    using ImpactFn = void (*)(RE::MagicCaster*, RE::NiPoint3*, RE::Projectile*, RE::TESObjectREFR*,
                             float, float, std::uint8_t, std::uint8_t);
    using AddFn = bool (*)(RE::MagicTarget*, RE::MagicTarget::AddTargetData&);
    inline static LaunchFn originalLaunch = nullptr;
    inline static REL::Relocation<ImpactFn> originalImpact;
    inline static AddFn originalAdd = nullptr;
    std::recursive_mutex mutex;
    IronLungsGrant grant;
    Overexertion overexertion;
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
            s->overexertion.shout();
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
        struct SpellScope {
            RE::MagicItem* previous;
            explicit SpellScope(RE::MagicItem* spell) : previous(std::exchange(launchingSpell, spell)) {}
            ~SpellScope() { launchingSpell = previous; }
        } spellScope(tag ? data.spell : nullptr);
        auto answer = originalLaunch(result, data);
        if (tag && answer && answer->get()) {
            std::lock_guard lock(s->mutex);
            s->shots.insert_or_assign(answer->native_handle(), Shot{*answer, releasing, data.spell});
            ++releasing->projectiles;
            if (s->logNext()) SKSE::log::info("[IronLungs] PROJECTILE {}: handle={:08X}, spell={:08X}",
                releasing->serial, answer->native_handle(), data.spell->GetFormID());
        }
        return answer;
    }
    static void impact(RE::MagicCaster* caster, RE::NiPoint3* position, RE::Projectile* projectile,
                       RE::TESObjectREFR* target, float power, float magnitude,
                       std::uint8_t noHitArt, std::uint8_t hostileOnly) {
        auto s = self;
        IronLungsImpact context;
        if (s && projectile && target && !applyingBonus) {
            std::lock_guard lock(s->mutex);
            const auto& data = projectile->GetProjectileRuntimeData();
            const auto shooter = data.shooter.get();
            auto player = RE::PlayerCharacter::GetSingleton();
            // Resolve by the exact handle and spell; overlapping shouts may
            // arrive out of order. NPC and instant/Skald projectiles stay untagged.
            if (s->selected() && player && shooter.get() == player &&
                data.castingSource == RE::MagicSystem::CastingSource::kOther && s->isForce(data.spell)) {
                const auto it = s->shots.find(projectile->GetHandle().native_handle());
                Cast cast;
                if (it != s->shots.end() && it->second.handle.get().get() == projectile &&
                    it->second.spell == data.spell) cast = it->second.cast;
                // Only an explicit impact of the currently launching spell
                // can use this initialization fallback, not a recent-cast timer.
                if (!cast && launching && launchingSpell == data.spell) cast = launching;
                context = {std::move(cast), data.spell->GetFormID(), player->GetFormID(), target->GetFormID()};
                if (s->logNext()) SKSE::log::info("[IronLungs] IMPACT: projectile={:08X}, spell={:08X}, target={:08X}, cast={}",
                    projectile->GetHandle().native_handle(), context.spell, context.target, context.cast ? context.cast->serial : 0);
            }
        }
        // Even an untracked nested impact must clear an outer cast context.
        IronLungsImpactScope hitScope(hitting, std::move(context));
        originalImpact(caster, position, projectile, target, power, magnitude, noHitArt, hostileOnly);
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
        // Incoming magnitude takes (target, spell, output), unlike outgoing
        // magnitude's (caster, spell, target, output). An extra actor argument
        // here would be interpreted by the engine as the float output pointer.
        RE::BGSEntryPoint::HandleEntryPoint(Entry::kModIncomingSpellMagnitude, target, originalSpell, &magnitude);
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
        const auto context = hitting;
        const bool bonus = applyingBonus;
        const bool accepted = originalAdd(target, data);
        if (s && target && data.magicItem) {
            // GetTargetAsActor in the pinned library does not adjust the AE
            // secondary-base pointer. Obtain the real reference virtually.
            auto ref = target->GetTargetStatsObject();
            auto actor = ref ? ref->As<RE::Actor>() : nullptr;
            const auto casterID = data.caster ? data.caster->GetFormID() : 0;
            const auto targetID = actor ? actor->GetFormID() : 0;
            const bool eligible = context.matches(data.magicItem->GetFormID(), casterID, targetID, accepted, bonus);
            if ((s->isForce(data.magicItem) || data.magicItem == s->bonusSpell) &&
                data.caster == RE::PlayerCharacter::GetSingleton()) {
                std::lock_guard lock(s->mutex);
                if (s->logNext()) SKSE::log::info("[IronLungs] APPLY: spell={:08X}, target={:08X}, accepted={}, cast={}, matched={}, bonus_effect={}, magnitude={}",
                    data.magicItem->GetFormID(), targetID, accepted, context.cast ? context.cast->serial : 0,
                    eligible, data.magicItem == s->bonusSpell, data.magnitude);
            }
            if (eligible) s->damage(actor, data.magicItem, context.cast);
        }
        return accepted;
    }
public:
    void reset() {
        std::lock_guard lock(mutex);
        grant.reset();
        overexertion.clear();
        shots.clear(); pruneAfter = 0; logBudget = 200; warnedUnavailable = false;
        lastFailure = {};
    }
    void tick(float dt) {
        std::lock_guard lock(mutex);
        grant.tick(dt);
        auto p = RE::PlayerCharacter::GetSingleton();
        overexertion.tick(dt, selected() && p && !p->IsDead());
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
    void manualShout(RE::TESShout* shout) {
        std::lock_guard lock(mutex);
        auto p = RE::PlayerCharacter::GetSingleton();
        // UF arms only when its release actually launches a projectile. Other
        // manual shouts use the normal VoiceFire event, like Echoing Steel.
        if (!shout || shout == unrelenting || !selected() || !p || p->IsDead() ||
            (skaldCasting && skaldCasting())) return;
        overexertion.shout();
        if (logNext()) SKSE::log::info("[IronLungs] Overexertion: manual shout {:08X}; 20% physical vulnerability for 3 seconds", shout->GetFormID());
    }
    float physicalPenalty(float total, float physical) {
        std::lock_guard lock(mutex);
        auto p = RE::PlayerCharacter::GetSingleton();
        if (!selected() || !p || p->IsDead()) { overexertion.clear(); return 0; }
        const float extra = overexertion.extraDamage(total, physical);
        if (extra > 0 && logNext()) SKSE::log::info("[IronLungs] Overexertion HIT: physical={}, total={}, extra={}, remaining={}",
            physical, total, extra, overexertion.remaining);
        return extra;
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
            if (!word.word || !word.spell || word.spell->GetDelivery() != RE::MagicSystem::Delivery::kAimed ||
                word.spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) {
                SKSE::log::error("Iron Lungs: unsupported UF delivery/casting override; trait disabled"); return false;
            }
            const bool hasProjectile = std::any_of(word.spell->effects.begin(), word.spell->effects.end(),
                [](RE::Effect* effect) { return effect && effect->baseEffect && effect->baseEffect->data.projectileBase; });
            if (!hasProjectile) {
                SKSE::log::error("Iron Lungs: UF spell {:08X} has no projectile; trait disabled", word.spell->GetFormID()); return false;
            }
        }
        // Explicit projectile hit, rather than release-time FindTargets.
        // AE callsite: NoahBoddie/perk-entry-expansion 2a75ca5d, MACS hook.
        // All eight arguments: Newrite/ReflyemSKSEPlugin f627a0ca, OnMagicHit;
        // also Valhalla Combat. Preserve the final two bytes when forwarding.
        const auto impactAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(43015, 44206)}.address() + 0x218;
        if (*reinterpret_cast<const std::uint8_t*>(impactAddress) != 0xE8) {
            SKSE::log::error("Iron Lungs: projectile impact call is not the expected E8; trait disabled"); return false;
        }
        // AddTarget AE 34526 is independently used by TiltedEvolution.
        const auto launchAddress = REL::Relocation<std::uintptr_t>{REL::RelocationID(42928, 44108)}.address();
        const auto addAddress = REL::Relocation<std::uintptr_t>{REL::ID(34526)}.address();
        struct Hook { std::uintptr_t address; void* replacement; void** original; };
        const Hook hooks[] = {
            {launchAddress, reinterpret_cast<void*>(launch), reinterpret_cast<void**>(&originalLaunch)},
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
        originalImpact = SKSE::GetTrampoline().write_call<5>(impactAddress, impact);
        SKSE::log::info("Iron Lungs: projectile impact hook installed at 44206+0x218; eight arguments forwarded");
        REL::Relocation<std::uintptr_t> casterTable{RE::VTABLE_ActorMagicCaster[0]};
        originalCheck = casterTable.write_vfunc(0xA, check);
        REL::Relocation<std::uintptr_t> voiceTable{RE::VTABLE_VoiceSpellFireHandler[0]};
        originalVoice = voiceTable.write_vfunc(0x1, voice);
        installed = true;
        grant.init(unrelenting, [this] { return selected(); });
        SKSE::log::info("Iron Lungs ready: magic bonus; strict Stamina gate; 25% maximum cost, 10% floor; 25% pre-payment current Stamina damage; manual shouts cause 20% physical vulnerability for 3 seconds; Skald exempt");
        return true;
    }
};
}
