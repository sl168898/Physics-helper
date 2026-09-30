#include "AmmoIconEffects.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    using namespace AmmoIcon;
    unsigned checked = 0;
    // Every valid native/coating pair must retain two independent types,
    // survive the DLL contract, and keep arrow vs bolt identity.
    for (bool bolt : {false, true}) for (unsigned native = 0; native <= 4; ++native)
        for (unsigned coating = 1; coating <= 4; ++coating) {
            State state{bolt, static_cast<Damage>(native), static_cast<Damage>(coating)};
            auto decoded = UnpackCoated(PackCoated(state));
            assert(decoded && decoded->Key() == state.Key());
            assert(decoded->Head() == (native ? state.native : state.coating));
            assert(decoded->NeedsNativeIcon()); ++checked;
        }
    assert(!UnpackCoated(0));
    assert(!UnpackCoated(apiTag));
    assert(!UnpackCoated(apiTag | 0x55));
    assert(!UnpackCoated(apiTag | 0x210));
    assert(!UnpackCoated(0xA2010011));
    assert(!State{}.NeedsNativeIcon());
    assert((State{true}.NeedsNativeIcon()));
    assert((State{true, Damage::Fire, Damage::Poison}.Filename() == "bolt_fire_poison.svg"));
    assert((State{false, Damage::None, Damage::Frost}.Filename() == "arrow_frost_frost.svg"));
    assert((State{true, Damage::Frost, Damage::Fire}.Key() != State{true, Damage::Fire, Damage::Frost}.Key()));

    RE::EffectSetting fire, frost, shock, health, weakness, resist, paralysis;
    fire.hostile = frost.hostile = shock.hostile = health.hostile = weakness.hostile = paralysis.hostile = true;
    fire.data.primaryAV = frost.data.primaryAV = shock.data.primaryAV = health.data.primaryAV = RE::ActorValue::kHealth;
    fire.data.resistVariable = RE::ActorValue::kResistFire;
    frost.keywords = {"MagicDamageFrost"};
    shock.data.resistVariable = RE::ActorValue::kResistShock;
    health.data.resistVariable = RE::ActorValue::kPoisonResist;
    weakness.data.primaryAV = RE::ActorValue::kResistFire;
    weakness.data.resistVariable = RE::ActorValue::kPoisonResist;
    resist.data.primaryAV = RE::ActorValue::kResistFrost; // Beneficial potion.
    paralysis.data.primaryAV = RE::ActorValue::kParalysis;
    assert(EffectType(&fire, true) == Damage::Fire);
    assert(EffectType(&frost, true) == Damage::Frost);
    assert(EffectType(&shock, true) == Damage::Shock);
    assert(EffectType(&health, true) == Damage::Poison);
    assert(EffectType(&weakness, true) == Damage::None); // Not fire damage.
    assert(EffectType(&resist, true) == Damage::None);
    assert(EffectType(&paralysis, true) == Damage::None);
    assert(MagicType(nullptr, true) == Damage::Poison);
    assert(MagicType(nullptr) == Damage::None);
    RE::Effect a{&fire, {12, 5}}, b{&frost, {20, 4}}, c{&weakness, {9999, 9999}};
    RE::MagicItem mixed{{&a, &b, &c}};
    assert(MagicType(&mixed, true) == Damage::Frost); // 80 vs 60, ignore weakness.
    b.effectItem.magnitude = 15;
    assert(MagicType(&mixed, true) == Damage::Fire); // Stable equal-score result.
    b.effectItem.magnitude = std::numeric_limits<float>::quiet_NaN();
    assert(MagicType(&mixed, true) == Damage::Fire);
    a.effectItem.magnitude = 0;
    assert(MagicType(&mixed, true) == Damage::Poison);
    a.effectItem.magnitude = 10;
    RE::MagicItem elemental{{&a}};
    RE::BGSExplosion explosion{&elemental};
    RE::BGSProjectile projectile;
    projectile.data.explosionType = &explosion;
    RE::TESAmmo ammo;
    ammo.runtime.data.projectile = &projectile;
    assert(NativeType(&ammo) == Damage::None); // Dormant pointer is not an explosion.
    projectile.data.flags.enabled = true;
    assert(NativeType(&ammo) == Damage::Fire);
    projectile.data.explosionType = nullptr;
    assert(NativeType(&ammo) == Damage::None);
    assert(NativeType(nullptr) == Damage::None);
    std::cout << "PASS: " << checked << " independent coating/native combinations; ABI rejection; health vs weakness; mixed effects; actual projectile-explosion traversal\n";
}
