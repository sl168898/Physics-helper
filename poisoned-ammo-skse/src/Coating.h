#pragma once
#include "CoatingCore.h"
#include "Potency.h"
#include <vector>

namespace coating
{
    inline RE::BGSPerk* rank1{};
    inline RE::BGSPerk* rank2{};
    inline RE::BGSPerk* measured{};
    inline bool trace{};
    inline bool available{};
    struct Context
    {
        RE::Actor* actor{};
        RE::AlchemyItem* poison{};
        float multiplier = 1;
        std::vector<RE::ActiveEffect*> adjusted;
    };
    inline thread_local Context* current{};
    inline thread_local RE::ActiveEffect* adjusting{};

    // Synchronous impact context identifies the weapon that fired this bolt,
    // even if the shooter has since switched weapons. Never inspect equipped
    // weapons to decide whether an unrelated poison application gets a bonus.
    class Scope
    {
        Context context;
        Context* previous = current;
        RE::NiPointer<RE::TESObjectREFR> shooter;
    public:
        explicit Scope(RE::ArrowProjectile* projectile, bool session)
        {
            // A nested non-bolt impact must not inherit the outer bolt's context.
            current = nullptr;
            if (!available || !session || !projectile) return;
            auto& runtime = projectile->GetProjectileRuntimeData();
            if (!runtime.weaponSource || !runtime.weaponSource->IsCrossbow() ||
                !runtime.ammoSource || !runtime.ammoSource->IsBolt()) return;
            shooter = runtime.shooter.get();
            context.actor = shooter ? shooter->As<RE::Actor>() : nullptr;
            context.poison = projectile->GetArrowRuntimeData().poison;
            if (!context.actor || !context.poison || !context.poison->IsPoison()) return;
            context.multiplier = strength(context.actor->HasPerk(rank1), context.actor->HasPerk(rank2));
            // Potency can be owned through Measured Dose without either
            // Mechanist rank. Retain the crossbow scope even at strength 1.
            current = &context;
            if (trace) SKSE::log::info("Coating impact: projectile={:08X}, shooter={:08X}, poison={:08X}, strength={}",
                projectile->GetFormID(), context.actor->GetFormID(), context.poison->GetFormID(), context.multiplier);
        }
        ~Scope() { current = previous; }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    template<class T> struct Hook
    {
        inline static REL::Relocation<void (*)(RE::ActiveEffect*, RE::Actor*, RE::MagicTarget*)> original;
        static void Adjust(RE::ActiveEffect* effect, RE::Actor* caster, RE::MagicTarget* target)
        {
            if (adjusting == effect) { original(effect, caster, target); return; }
            struct Guard {
                RE::ActiveEffect* before;
                explicit Guard(RE::ActiveEffect* value) : before(adjusting) { adjusting = value; }
                ~Guard() { adjusting = before; }
            } guard(effect);
            original(effect, caster, target);
            const auto ctx = current;
            if (!ctx || !effect || caster != ctx->actor || effect->spell != ctx->poison ||
                !effect->effect || !effect->effect->baseEffect || !target ||
                target->GetTargetAsActor() == caster) return;
            if (std::find(ctx->adjusted.begin(), ctx->adjusted.end(), effect) != ctx->adjusted.end()) return;
            ctx->adjusted.push_back(effect);
            // Only this exact crossbow impact's oil effect receives Potency.
            // The same per-impact guard protects both perk multipliers.
            potency::apply(effect, caster, target);
            const auto base = effect->effect->baseEffect;
            // Pure script/marker effects with no magnitude or duration are unchanged.
            const bool noMagnitude = base->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
            const float oldMagnitude = effect->magnitude, oldDuration = effect->duration;
            if (scale(effect->magnitude, effect->duration, noMagnitude, ctx->multiplier) && trace)
                SKSE::log::info("Coating applied: poison={:08X}, effect={:08X}, magnitude {} -> {}, duration {} -> {}",
                    ctx->poison->GetFormID(), base->GetFormID(), oldMagnitude, effect->magnitude, oldDuration, effect->duration);
        }
        static void install()
        {
            REL::Relocation<std::uintptr_t> table{T::VTABLE[0]};
            original = table.write_vfunc(0x00, Adjust);
        }
    };
    inline void install(RE::TESDataHandler* data, bool logging)
    {
        rank1 = data->LookupForm<RE::BGSPerk>(0x800, "CoatingMechanist.esp");
        rank2 = data->LookupForm<RE::BGSPerk>(0x801, "CoatingMechanist.esp");
        measured = data->LookupForm<RE::BGSPerk>(0x802, "CoatingMechanist.esp");
        available = rank1 && rank2 && measured;
        trace = logging;
        if (!available) { SKSE::log::info("Optional CoatingMechanist.esp absent; coating perks disabled"); return; }
        Hook<RE::AbsorbEffect>::install();
        Hook<RE::AccumulatingValueModifierEffect>::install();
        Hook<RE::ActiveEffect>::install();
        Hook<RE::BanishEffect>::install();
        Hook<RE::BoundItemEffect>::install();
        Hook<RE::CalmEffect>::install();
        Hook<RE::CloakEffect>::install();
        Hook<RE::CommandEffect>::install();
        Hook<RE::CommandSummonedEffect>::install();
        Hook<RE::ConcussionEffect>::install();
        Hook<RE::CureEffect>::install();
        Hook<RE::DarknessEffect>::install();
        Hook<RE::DemoralizeEffect>::install();
        Hook<RE::DetectLifeEffect>::install();
        Hook<RE::DisarmEffect>::install();
        Hook<RE::DisguiseEffect>::install();
        Hook<RE::DispelEffect>::install();
        Hook<RE::DualValueModifierEffect>::install();
        Hook<RE::EnhanceWeaponEffect>::install();
        Hook<RE::EtherealizationEffect>::install();
        Hook<RE::FrenzyEffect>::install();
        Hook<RE::GrabActorEffect>::install();
        Hook<RE::GuideEffect>::install();
        Hook<RE::InvisibilityEffect>::install();
        Hook<RE::LightEffect>::install();
        Hook<RE::LockEffect>::install();
        Hook<RE::NightEyeEffect>::install();
        Hook<RE::OpenEffect>::install();
        Hook<RE::ParalysisEffect>::install();
        Hook<RE::PeakValueModifierEffect>::install();
        Hook<RE::RallyEffect>::install();
        Hook<RE::ReanimateEffect>::install();
        Hook<RE::ScriptEffect>::install();
        Hook<RE::ScriptedRefEffect>::install();
        Hook<RE::SlowTimeEffect>::install();
        Hook<RE::SoulTrapEffect>::install();
        Hook<RE::SpawnHazardEffect>::install();
        Hook<RE::StaggerEffect>::install();
        Hook<RE::SummonCreatureEffect>::install();
        Hook<RE::TelekinesisEffect>::install();
        Hook<RE::TurnUndeadEffect>::install();
        Hook<RE::ValueAndConditionsEffect>::install();
        Hook<RE::ValueModifierEffect>::install();
        Hook<RE::VampireLordEffect>::install();
        Hook<RE::WerewolfEffect>::install();
        Hook<RE::WerewolfFeedEffect>::install();
        SKSE::log::info("Coating perks enabled: I={:08X}, II={:08X}, Measured Dose={:08X}; per-impact instance scaling",
            rank1->GetFormID(), rank2->GetFormID(), measured->GetFormID());
    }
}
