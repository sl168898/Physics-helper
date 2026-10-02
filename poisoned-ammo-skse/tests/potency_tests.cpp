// Compile the production discovery, eligibility and impact hooks against small
// engine doubles. Windows CI separately compiles these headers with CommonLib.
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RE {
    using FormID = std::uint32_t;
    enum class ActorValue { kHealth, kAlchemy, kStamina, kResistFire };
    struct TESForm {
        FormID id{};
        virtual ~TESForm() = default;
        FormID GetFormID() const { return id; }
        template<class T> T* As() { return dynamic_cast<T*>(this); }
    };
    struct BGSPerk : TESForm {};
    struct TESObjectREFR : TESForm {};
    struct Actor : TESObjectREFR {
        float alchemy{};
        std::unordered_set<BGSPerk*> perks;
        bool HasPerk(BGSPerk* p) const { return p && perks.contains(p); }
        Actor* AsActorValueOwner() { return this; }
        float GetActorValue(ActorValue av) const { assert(av == ActorValue::kAlchemy); return alchemy; }
    };
    struct PlayerCharacter : Actor {
        inline static PlayerCharacter* instance{};
        static PlayerCharacter* GetSingleton() { return instance; }
    };
    struct MagicTarget {
        Actor* actor{};
        Actor* GetTargetAsActor() { return actor; }
    };
    struct EffectSetting : TESForm {
        enum class Archetype { kValueModifier, kDualValueModifier, kScript, kStagger };
        struct EffectSettingData {
            enum class Flag { kDetrimental = 4, kNoMagnitude = 1024 };
            struct Flags {
                unsigned value = 4;
                bool any(Flag f) const { return (value & static_cast<unsigned>(f)) != 0; }
            } flags;
            Archetype archetype = Archetype::kValueModifier;
            ActorValue primaryAV = ActorValue::kHealth;
        } data;
    };
    struct Effect { EffectSetting* baseEffect{}; float magnitude = 20, duration = 5; };
    struct MagicItem : TESForm {};
    struct AlchemyItem : MagicItem {
        bool poison = true;
        std::vector<Effect*> effects;
        bool IsPoison() const { return poison; }
        const char* GetName() const { return "oil fixture"; }
    };
    struct ScrollItem : MagicItem {};
    struct ActiveEffect {
        inline static constexpr std::uintptr_t VTABLE[] = {1};
        MagicItem* spell{}; Effect* effect{};
        float magnitude = 20, duration = 5;
    };
    // Give every installed production hook a distinct type/slot.
#define EFFECT(T) struct T : ActiveEffect {};
    EFFECT(AbsorbEffect) EFFECT(AccumulatingValueModifierEffect) EFFECT(BanishEffect)
    EFFECT(BoundItemEffect) EFFECT(CalmEffect) EFFECT(CloakEffect) EFFECT(CommandEffect)
    EFFECT(CommandSummonedEffect) EFFECT(ConcussionEffect) EFFECT(CureEffect)
    EFFECT(DarknessEffect) EFFECT(DemoralizeEffect) EFFECT(DetectLifeEffect)
    EFFECT(DisarmEffect) EFFECT(DisguiseEffect) EFFECT(DispelEffect)
    EFFECT(DualValueModifierEffect) EFFECT(EnhanceWeaponEffect) EFFECT(EtherealizationEffect)
    EFFECT(FrenzyEffect) EFFECT(GrabActorEffect) EFFECT(GuideEffect) EFFECT(InvisibilityEffect)
    EFFECT(LightEffect) EFFECT(LockEffect) EFFECT(NightEyeEffect) EFFECT(OpenEffect)
    EFFECT(ParalysisEffect) EFFECT(PeakValueModifierEffect) EFFECT(RallyEffect)
    EFFECT(ReanimateEffect) EFFECT(ScriptEffect) EFFECT(ScriptedRefEffect)
    EFFECT(SlowTimeEffect) EFFECT(SoulTrapEffect) EFFECT(SpawnHazardEffect)
    EFFECT(StaggerEffect) EFFECT(SummonCreatureEffect) EFFECT(TelekinesisEffect)
    EFFECT(TurnUndeadEffect) EFFECT(ValueAndConditionsEffect) EFFECT(ValueModifierEffect)
    EFFECT(VampireLordEffect) EFFECT(WerewolfEffect) EFFECT(WerewolfFeedEffect)
#undef EFFECT
    template<class T> struct NiPointer {
        T* p{};
        T* operator->() const { return p; }
        explicit operator bool() const { return p != nullptr; }
    };
    struct ObjectRefHandle {
        TESObjectREFR* p{};
        NiPointer<TESObjectREFR> get() const { return {p}; }
    };
    struct TESObjectWEAP { bool crossbow = true; bool IsCrossbow() const { return crossbow; } };
    struct TESAmmo { bool bolt = true; bool IsBolt() const { return bolt; } };
    struct ArrowProjectile : TESForm {
        struct Runtime { TESObjectWEAP* weaponSource{}; TESAmmo* ammoSource{}; ObjectRefHandle shooter; } runtime;
        struct ArrowRuntime { AlchemyItem* poison{}; } arrow;
        Runtime& GetProjectileRuntimeData() { return runtime; }
        ArrowRuntime& GetArrowRuntimeData() { return arrow; }
    };
    struct TESDataHandler {
        std::map<std::pair<std::string, FormID>, TESForm*> forms;
        template<class T> T* LookupForm(FormID id, const char* file) {
            auto it = forms.find({file, id});
            return it == forms.end() ? nullptr : dynamic_cast<T*>(it->second);
        }
    };
}
namespace SKSE::log {
    template<class... T> void info(const char*, T&&...) {}
    template<class... T> void warn(const char*, T&&...) {}
}
namespace REL {
    template<class F> struct Relocation {
        F function{};
        Relocation() = default;
        explicit Relocation(F f) : function(f) {}
        Relocation& operator=(F f) { function = f; return *this; }
        template<class... T> decltype(auto) operator()(T&&... args) { return function(std::forward<T>(args)...); }
        template<class T> T write_vfunc(unsigned, T f) { return f; }
    };
}
#include "Coating.h"

using Hook = coating::Hook<RE::ActiveEffect>;
using DerivedHook = coating::Hook<RE::ValueModifierEffect>;
int originalCalls{};
float nativeMultiplier = 1;
void original(RE::ActiveEffect* e, RE::Actor*, RE::MagicTarget*) {
    ++originalCalls;
    if (e) e->magnitude *= nativeMultiplier;
}
void derivedOriginal(RE::ActiveEffect* e, RE::Actor* a, RE::MagicTarget* t) { Hook::Adjust(e, a, t); }
void near(float a, float b) { assert(std::abs(a - b) < 0.001f); }

int main() {
    RE::PlayerCharacter player; RE::PlayerCharacter::instance = &player;
    RE::Actor enemy, npc; RE::MagicTarget target{&enemy}, self{&player};
    RE::BGSPerk perk, rank1, rank2, measured;
    RE::TESDataHandler data;
    for (auto [id, p] : std::initializer_list<std::pair<RE::FormID, RE::BGSPerk*>>{
        {0x800,&rank1}, {0x801,&rank2}, {0x802,&measured}, {0x803,&perk}})
        data.forms[{"CoatingMechanist.esp",id}] = p;
    RE::EffectSetting fire, species, weakness, stagger, ordinary;
    species.data.archetype = RE::EffectSetting::Archetype::kDualValueModifier;
    weakness.data.primaryAV = RE::ActorValue::kResistFire;
    stagger.data.archetype = RE::EffectSetting::Archetype::kStagger;
    RE::Effect direct{&fire}, conditional{&species}, status{&weakness}, ancillary{&stagger}, poisonEffect{&ordinary};
    RE::AlchemyItem oil, greater, poison, proxy;
    oil.effects = {&direct,&conditional,&status,&ancillary}; greater.effects = oil.effects;
    proxy.effects = oil.effects; // Saved coating proxies retain original effect pointers.
    data.forms[{"Requiem - Alchemy Redone.esp",0x807}] = &oil;
    data.forms[{"Big Tweaks.esp",0xA6D}] = &greater;
    potency::install(&data, true); coating::install(&data, true);
    assert(potency::perk == &perk && potency::oilDamage.size() == 2 && coating::available);
    assert(potency::oilDamage.contains(&fire) && potency::oilDamage.contains(&species));
    Hook::original = original; DerivedHook::original = derivedOriginal;
    RE::TESObjectWEAP crossbow, bow; bow.crossbow = false;
    RE::TESAmmo bolt, arrow; arrow.bolt = false;
    RE::ArrowProjectile shot;
    shot.runtime = {&crossbow,&bolt,{&player}}; shot.arrow.poison = &oil;
    player.perks = {&perk,&measured};
    auto active = [&](RE::MagicItem* source, RE::Effect* effect, float mag = 20) {
        RE::ActiveEffect a; a.spell = source; a.effect = effect; a.magnitude = mag; return a;
    };
    auto impact = [&](RE::ActiveEffect& a, RE::Actor* caster = nullptr, RE::MagicTarget* recipient = nullptr, bool session = true) {
        coating::Scope scope(&shot,session);
        Hook::Adjust(&a,caster ? caster : &player,recipient ? recipient : &target);
    };
    // Current Alchemy is read at impact. No Mechanist rank is required.
    for (float skill : {0.f,25.f,50.f,100.f,150.f}) {
        player.alchemy = skill; auto a = active(&oil,&direct); impact(a);
        near(a.magnitude,20*(1+skill/100)); near(a.duration,5);
    }
    player.alchemy = 100;
    shot.arrow.poison = &greater;
    auto big = active(&greater,&direct,40); impact(big); near(big.magnitude,80);
    auto extra = active(&greater,&conditional,80); impact(extra); near(extra.magnitude,160);
    shot.arrow.poison = &proxy;
    auto saved = active(&proxy,&direct); impact(saved); near(saved.magnitude,40);
    shot.arrow.poison = &oil;
    auto a = active(&oil,&direct);
    Hook::Adjust(&a,&player,&target); near(a.magnitude,20); // Melee/no impact context.
    shot.runtime.weaponSource = &bow; impact(a); near(a.magnitude,20);
    shot.runtime.weaponSource = &crossbow; shot.runtime.ammoSource = &arrow; impact(a); near(a.magnitude,20);
    shot.runtime.ammoSource = &bolt; impact(a,nullptr,nullptr,false); near(a.magnitude,20);
    impact(a,&npc); near(a.magnitude,20); impact(a,nullptr,&self); near(a.magnitude,20);
    player.perks.clear(); impact(a); near(a.magnitude,20); player.perks.insert(&perk);
    shot.arrow.poison = &poison; a = active(&poison,&poisonEffect); impact(a); near(a.magnitude,20);
    shot.arrow.poison = &oil; a = active(&greater,&direct); impact(a); near(a.magnitude,20); // Wrong native poison.
    RE::ScrollItem powder; a = active(&powder,&direct); impact(a); near(a.magnitude,20);
    for (auto e : {&status,&ancillary}) { a = active(&oil,e); impact(a); near(a.magnitude,20); }
    fire.data.flags.value = 0; a = active(&oil,&direct); impact(a); near(a.magnitude,20);
    fire.data.flags.value = 4|1024; a = active(&oil,&direct); impact(a); near(a.magnitude,20); near(a.duration,5);
    fire.data.flags.value = 4;
    // Engine adjustment, then Alchemy, then the existing Mechanist multiplier.
    nativeMultiplier = 1.1f; player.perks.insert(&rank1);
    a = active(&oil,&direct); impact(a); near(a.magnitude,55);
    player.perks.insert(&rank2); a = active(&oil,&direct); impact(a); near(a.magnitude,66);
    player.perks.erase(&perk); a = active(&oil,&direct); impact(a); near(a.magnitude,33);
    player.perks = {&perk}; nativeMultiplier = 1;
    // Derived AdjustForPerks calls its base: exactly one native call/bonus.
    a = active(&oil,&direct); originalCalls = 0;
    {
        coating::Scope outer(&shot,true);
        DerivedHook::Adjust(&a,&player,&target);
        near(a.magnitude,40); assert(originalCalls == 1);
        Hook::Adjust(&a,&player,&target); near(a.magnitude,40); // Per-impact duplicate.
        auto b = active(&oil,&direct);
        { coating::Scope unrelated(nullptr,true); Hook::Adjust(&b,&player,&target); near(b.magnitude,20); }
        Hook::Adjust(&b,&player,&target); near(b.magnitude,40); // Outer scope restored.
    }
    assert(!coating::current && !coating::adjusting);
    near(direct.magnitude,20); near(direct.duration,5); // Shared record untouched.
    for (float skill : {-5.f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        player.alchemy = skill; a = active(&oil,&direct); impact(a); near(a.magnitude,20);
    }
    float magnitude = std::numeric_limits<float>::max();
    assert(!potency::scaleDamage(magnitude,100));
    magnitude = -20; assert(potency::scaleDamage(magnitude,100)); near(magnitude,-40);
    RE::TESDataHandler absent; potency::install(&absent,false);
    assert(!potency::perk && potency::oilDamage.empty());
}
