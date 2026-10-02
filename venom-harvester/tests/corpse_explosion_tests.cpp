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
    auto first = ledger.claim(1,50); assert(first && (*first)[0] == 375 && (*first)[3] == 125);
    assert(!ledger.claim(1,50) && !ledger.killed(1, fire));
    ledger.record(1, fire, 500); assert(ledger.targets.at(1).total == 800);
    ledger.resurrected(1); ledger.record(1, fire, 100);
    assert(ledger.killed(1, fire) && ledger.claim(1,50)->at(0) == 150);

    // Weapon, follower and unqualified explosion kills cannot burst.
    for (const auto killer : {weapon, follower, blast}) {
        Ledger rejected; rejected.record(2, fire, 100);
        assert(!rejected.killed(2, killer)); assert(!rejected.claim(2,50));
        assert(!rejected.killed(2, fire));
    }
    // Only the lethal oil controls the mix; older different oils cannot add types.
    Ledger mixed;
    mixed.record(3, Origin{true, false, 20, Type::frost}, 600);
    mixed.record(3, fire, 400); assert(mixed.killed(3, fire));
    assert(mixed.claim(3,50)->at(0) == 600);

    // A 5,000-damage poison hitting a 200-Health enemy records only 200 lost
    // Health: 50% + 100 produces 200 damage before recipient resistance.
    Ledger overkill;
    overkill.record(4, venom, healthLost(200, -4800));
    assert(overkill.killed(4, venom) && overkill.claim(4,50)->at(3) == 200);

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
    const auto legacyBytes=[](const Ledger& value,unsigned version) {
        auto bytes=encode(value); assert(value.targets.size()==1);
        bytes.erase(bytes.begin()+56,bytes.begin()+60); // v1-v3 had no remaining field.
        bytes[0]=static_cast<std::uint8_t>(version); return bytes;
    };
    Ledger saved; saved.record(42, fire, 120); assert(saved.killed(42, fire));
    auto bytes = encode(saved);
    auto restored = decode(bytes, [](ID id) { return id + 1; });
    assert(restored && restored->claim(43,50)->at(0) == 160 && !restored->claim(43,50));
    auto spent = decode(encode(*restored), [](ID id) { return id; });
    assert(spent && !spent->claim(43,50));
    for (std::size_t size = 0; size < bytes.size(); ++size)
        assert(!decode(std::span(bytes.data(), size), [](ID id) { return id; }));
    bytes.push_back(0); assert(!decode(bytes, [](ID id) { return id; }));
    auto corrupt = encode(saved); for (unsigned j = 0; j < 8; ++j) corrupt[16 + j] = 255;
    assert(!decode(corrupt, [](ID id) { return id; }));
    auto missing = decode(encode(saved), [](ID) { return ID{}; });
    assert(missing && missing->targets.empty());
    // v1 pending bursts normalize; level 50 adds its 100 bonus once at claim.
    for (double oldAmount : {30.0,60.0}) {
        Ledger legacy = saved; legacy.targets.at(42).blast[0] = oldAmount;
        auto v1 = legacyBytes(legacy,1);
        auto previous = decode(v1, [](ID id) { return id; });
        assert(previous);
        auto pendingAgain = decode(encode(*previous), [](ID id) { return id; });
        assert(pendingAgain && pendingAgain->claim(42,50)->at(0) == 160);
        assert(previous && previous->claim(42,50)->at(0) == 160);
        assert(!previous->claim(42,50));
        auto again = decode(encode(*previous), [](ID id) { return id; });
        assert(again && !again->claim(42,50));
    }
    Ledger excessive = saved; excessive.targets.at(42).blast[0] = 61;
    assert(!decode(encode(excessive), [](ID id) { return id; }));
    auto v1 = legacyBytes(excessive,1);
    assert(!decode(v1, [](ID id) { return id; }));
    Ledger oldSpent = saved; oldSpent.claim(42,50); oldSpent.targets.at(42).blast[0] = 60;
    v1 = legacyBytes(oldSpent,1);
    auto spentV1 = decode(v1, [](ID id) { return id; });
    assert(spentV1 && !spentV1->claim(42,50));
    // The undelivered fixed-100 build wrote v2. Strip that bonus on import,
    // preserve the type mix, then apply today's tier exactly once.
    Ledger fixed = saved; fixed.targets.at(42).blast[0] = 160;
    auto v2 = legacyBytes(fixed,2);
    auto migrated = decode(v2, [](ID id) { return id; });
    assert(migrated && migrated->targets.at(42).blast[0] == 60);
    auto migratedAgain = decode(encode(*migrated), [](ID id) { return id; });
    assert(migratedAgain && migratedAgain->claim(42,100)->at(0) == 260);
    assert(!migratedAgain->claim(42,100));
    assert(migrated->targets.at(42).remaining==0);
    auto v3=decode(legacyBytes(saved,3),[](ID id){return id;});
    unsigned budget=99;
    assert(v3 && v3->claim(42,100,&budget) && budget==0);
    auto invalidBudget=encode(saved); invalidBudget[56]=6;
    assert(!decode(invalidBudget,[](ID id){return id;}));
    auto v1Seed=decode(legacyBytes(saved,1),[](ID id){return id;});
    assert(v1Seed && v1Seed->claim(42,100,&budget) && budget==4);
    std::cout << "Corpse Explosion: 50% + Alchemy tiers, capped overkill, attribution, nested accounting, resistance, deaths and v1/v2/v3/v4 save tests passed\n";
}
