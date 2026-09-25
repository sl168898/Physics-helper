#include "HarvestYield.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace harvest;
constexpr ID poison = 0xFF001001, nextPoison = 0xFF001002;
const Recipe recipe{0x100, 0x200, 0x300};
const Ingredients cost{{0x100, 1}, {0x200, 2}, {0x300, 1}};
const auto exists = [](ID) { return true; };
Ledger prepared()
{
    Ledger result;
    result.sequence = 2;
    result.giftGiven = true;
    assert(result.remember(recipe));
    assert(result.add({poison, 1, recipe, cost, false}));
    assert(result.add({nextPoison, 2, recipe, cost, false}));
    return result;
}
int main()
{
    assert(doublesHarvestYield(87, 1, 2));
    assert(doublesHarvestYield(87, 2, 1));
    assert(doublesHarvestYield(87, 3, 2));
    assert(doublesHarvestYield(87, 1, 3));
    assert(!doublesHarvestYield(89, 1, 2)); // Extra bottles are not harvesting.
    assert(!doublesHarvestYield(83, 1, 2)); // Extra weapon hits are not harvesting.
    assert(!doublesHarvestYield(87, 1, 1));
    assert(!doublesHarvestYield(87, 3, 1.5f));
    assert(!doublesHarvestYield(87, 2, 0.5f));
    assert(!doublesHarvestYield(87, 4, 2)); // Random/AV/script rules are not guessed.
    assert(!doublesHarvestYield(87, 1, std::numeric_limits<float>::infinity()));
    assert(!doublesHarvestYield(87, 3, std::numeric_limits<float>::quiet_NaN()));
    {
        auto ledger = prepared();
        assert(!claimRefund(ledger, 10, true, exists)); // No proven poison kill.
        assert(ledger.offer(10, poison));
        assert(claimRefund(ledger, 10, false, exists) == cost);
        assert(ledger.offer(20, nextPoison));
        assert(claimRefund(ledger, 20, true, exists) == Ingredients({{0x100, 2}, {0x200, 4}, {0x300, 2}}));
        assert(ledger.batches.at(nextPoison).cost == cost);
        assert(ledger.giftGiven); // The one-time gift is not another refund.
    }
    {
        auto ledger = prepared();
        for (ID victim = 10; victim != 30; ++victim) assert(ledger.offer(victim, poison));
        assert(claimRefund(ledger, 10, true, exists));
        for (ID victim = 10; victim != 30; ++victim) {
            assert(!claimRefund(ledger, victim, true, exists));
            assert(!ledger.offer(victim, poison));
        }
        assert(ledger.unclaimed() == 1);
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, poison));
        auto saved = decode(encode(ledger), [](ID id) { return id; });
        assert(saved);
        // Taking the perk after brewing/loading affects an unpaid old batch.
        assert(claimRefund(*saved, 10, true, exists) == Ingredients({{0x100, 2}, {0x200, 4}, {0x300, 2}}));
        auto reloaded = decode(encode(*saved), [](ID id) { return id; });
        assert(reloaded && reloaded->giftGiven);
        assert(!reloaded->offer(11, poison)); // No replay after a save/load.
        assert(reloaded->offer(12, nextPoison));
        // Removing the perk before the refund returns only the recorded cost.
        assert(claimRefund(*reloaded, 12, false, exists) == cost);
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, poison));
        assert(!claimRefund(ledger, 10, true, [](ID id) { return id != 0x200; }));
        assert(ledger.eligible(poison));
        assert(ledger.offer(11, poison));
        assert(claimRefund(ledger, 11, true, exists));
        // Only the consumed subset is refunded when an ingredient was saved.
        Ledger partial;
        partial.sequence = 1;
        assert(partial.remember(recipe));
        assert(partial.add({poison, 1, recipe, {{0x100, 100000}}, false}));
        assert(partial.offer(12, poison));
        assert(claimRefund(partial, 12, true, exists) == Ingredients({{0x100, 200000}}));
        assert(partial.batches.at(poison).cost == Ingredients({{0x100, 100000}}));
        assert(decode(encode(partial), [](ID id) { return id; }));
    }
    std::cout << "Satchel harvest yield: 1x/2x costs, one allowance across victims, perk changes, save/load, partial costs and overflow bounds passed\n";
}
