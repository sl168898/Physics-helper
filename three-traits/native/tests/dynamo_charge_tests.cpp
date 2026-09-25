#include "ArcaneDynamo.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    using namespace traits::dynamo;
    int player{}, npc{}, fire{}, frost{}, staff{};
    const CostContext fireCast{&player, &fire}, frostCast{&player, &frost};
    const CostContext* active{};
    bool readingOriginal{};
    const auto costQuery = [&](const void* actor, const void* item) {
        return nativeChargeCost(34.f, active, actor, item, readingOriginal);
    };
    // Values/prices and unmanaged enchantments must retain their normal cost.
    assert(costQuery(&player, &fire) == 34.f);
    const CostContext invalid{};
    assert(nativeChargeCost(34.f, &invalid, &player, &fire, false) == 34.f);

    const auto hit = [&](bool releaseFirst, float magicka) {
        float charge = 100.f;
        unsigned payments{};
        bool delivered{};
        const auto release = [&] {
            Scope<const CostContext*> scope(active, &fireCast);
            charge -= costQuery(&player, &fire);
        };
        const auto deliver = [&] {
            Scope<const CostContext*> scope(active, &fireCast);
            float amount{};
            {
                Scope<bool> probe(readingOriginal, true);
                // The reported 15% reduction must still make 15 cost 12.75.
                amount = cost(40.f, costQuery(&player, &fire));
                assert(std::abs(amount - 12.75f) < 0.0001f);
            }
            if (!canPay(magicka, amount)) return;
            magicka -= amount;
            ++payments;
            // Two effects in one delivery do not create a second payment.
            delivered = damage(35.f, true) > 35.f && damage(20.f, true) > 20.f;
            assert(costQuery(&player, &fire) == 0.f);
            assert(costQuery(&npc, &fire) == 34.f); // same shared enchantment, different caster
            assert(costQuery(&player, &staff) == 34.f);
            assert(costQuery(nullptr, &fire) == 34.f); // actor-independent value
            {
                Scope<const CostContext*> nested(active, &frostCast);
                assert(costQuery(&player, &frost) == 0.f);
                assert(costQuery(&player, &fire) == 34.f);
            }
            assert(costQuery(&player, &fire) == 0.f);
        };
        if (releaseFirst) { release(); deliver(); } else { deliver(); release(); }
        assert(charge == 100.f);
        assert(payments == (delivered ? 1u : 0u));
        assert(active == nullptr && !readingOriginal);
        assert(costQuery(&player, &fire) == 34.f);
        return delivered;
    };
    assert(hit(true, 100.f)); // release before delivery: formerly receipt-dependent
    assert(hit(false, 100.f));
    assert(!hit(true, 12.f)); // no enchantment or charge when Magicka is insufficient
    assert(!hit(false, 12.f));
    // The empty charge check sees zero native cost while Magicka remains payable.
    { Scope<const CostContext*> scope(active, &fireCast);
      assert(0.f >= costQuery(&player, &fire)); }
    std::cout << "Dynamo charge: both release orders, original-cost payment, insufficient Magicka, empty charge, nested enchants, NPC/shared-form and out-of-cast isolation passed\n";
}
