#include "CorpseExplosion.h"
#include "DamageObservation.h"
#include <cassert>
#include <iostream>

using namespace corpse;
int main()
{
    const Origin weapon{true, false, 0, Type::poison};
    const Origin fire{true, false, 10, Type::fire};
    const Origin venom{true, false, 10, Type::poison};
    const Origin follower{false, false, 10, Type::fire};
    const Origin blast{true, true, 0, Type::fire};
    assert(healthLost(100, -1000) == 100);
    assert(healthLost(50, 100) == 0);
    assert(healthLost(0, -10) == 0);
    assert(healthLost(100, std::numeric_limits<double>::quiet_NaN()) == 0);
    assert(resistanceMultiplier(100) == 0 && resistanceMultiplier(150) == 0);
    assert(resistanceMultiplier(50) == .5 && resistanceMultiplier(-50) == 1.5);

    Ledger ledger;
    ledger.record(1, weapon, 400); ledger.record(1, fire, 300); ledger.record(1, venom, 100);
    ledger.record(1, follower, 2000); ledger.record(1, blast, 5000);
    assert(ledger.targets.at(1).total == 800);
    assert(ledger.killed(1, fire));
    auto first = ledger.claim(1); assert(first && (*first)[0] == 150 && (*first)[3] == 50);
    assert(!ledger.claim(1) && !ledger.killed(1, fire));
    ledger.record(1, fire, 500); assert(ledger.targets.at(1).total == 800);
    ledger.resurrected(1); ledger.record(1, fire, 100);
    assert(ledger.killed(1, fire) && ledger.claim(1)->at(0) == 25);

    // A coated target killed by a weapon, follower or explosion cannot burst.
    for (const auto killer : {weapon, follower, blast}) {
        Ledger rejected; rejected.record(2, fire, 100);
        assert(!rejected.killed(2, killer)); assert(!rejected.claim(2));
        assert(!rejected.killed(2, fire));
    }
    // Only the lethal oil controls the mix; older different oils cannot add types.
    Ledger mixed;
    mixed.record(3, Origin{true, false, 20, Type::frost}, 600);
    mixed.record(3, fire, 400); assert(mixed.killed(3, fire));
    assert(mixed.claim(3)->at(0) == 250);

    // Three overlapping scopes, including the generic native health callback.
    Frame outer{5, 100, 0, fire};
    Frame inner{5, 100, 0, fire, &outer};
    Frame native{5, 100, 0, fire, &inner};
    auto n = native.finish(-100, true); assert(n.damage == 100 && !n.death);
    auto i = inner.finish(-100, true); assert(i.damage == 0 && !i.death);
    auto o = outer.finish(-100, true); assert(o.damage == 0 && o.death && o.death->poison == 10);
    // A nested unrelated actor must not steal this actor's damage.
    Frame a{6, 100, 0, weapon}; Frame b{7, 500, 0, fire, &a};
    assert(b.finish(400, false).damage == 100); assert(a.finish(75, false).damage == 25);
    // Outer damage preceding the inner killing blow is counted before settlement.
    Frame early{8, 100, 0, fire}; Frame late{8, 60, 0, venom, &early};
    auto l = late.finish(-10, true); auto e = early.finish(-10, true);
    assert(l.damage == 60 && e.damage == 40 && e.death->type == Type::poison);
    // A nested follower kill remains a follower kill, even inside a player effect.
    Frame playerFrame{9, 100, 0, fire}; Frame followerFrame{9, 80, 0, follower, &playerFrame};
    followerFrame.finish(0, true); auto p = playerFrame.finish(0, true);
    assert(p.damage == 20 && p.death && !p.death->player);

    // Queued death is valid; essential bleedout and pre-existing death are not.
    assert(harvest::isLethalHealthChange({true, true, 10, 0, false, true, false, false}));
    assert(!harvest::isLethalHealthChange({true, true, 10, 0, false, true, false, true}));
    assert(!harvest::isLethalHealthChange({true, true, 10, 0, true, true, true, false}));
    Ledger saved; saved.record(42, fire, 120); assert(saved.killed(42, fire));
    auto bytes = encode(saved);
    auto restored = decode(bytes, [](ID id) { return id + 1; });
    assert(restored && restored->claim(43)->at(0) == 30 && !restored->claim(43));
    auto spent = decode(encode(*restored), [](ID id) { return id; });
    assert(spent && !spent->claim(43));
    for (std::size_t size = 0; size < bytes.size(); ++size)
        assert(!decode(std::span(bytes.data(), size), [](ID id) { return id; }));
    bytes.push_back(0); assert(!decode(bytes, [](ID id) { return id; }));
    auto corrupt = encode(saved); for (unsigned j = 0; j < 8; ++j) corrupt[16 + j] = 255;
    assert(!decode(corrupt, [](ID id) { return id; }));
    auto missing = decode(encode(saved), [](ID) { return ID{}; });
    assert(missing && missing->targets.empty());
    std::cout << "Corpse Explosion: attribution, nested accounting, mixed damage, resistance, deaths and save tests passed\n";
}
