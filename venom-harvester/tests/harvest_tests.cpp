#include "Harvest.h"
#include <cassert>
#include <iostream>
using namespace harvest;
constexpr ID a = 0xFF000100, b = 0xFF000101, c = 0xFF000102;
const Recipe roots{0x100, 0x200};
const Recipe flowers{0x300, 0x400, 0x500};
Ledger prepared()
{
    Ledger result;
    result.sequence = 3;
    assert(result.remember(roots));
    assert(result.add({a, 1, roots, {{0x100, 1}, {0x200, 1}}, false}));
    assert(result.add({b, 2, roots, {{0x100, 1}, {0x200, 2}}, false}));
    assert(result.add({c, 3, flowers, {{0x300, 1}, {0x400, 1}, {0x500, 1}}, false}));
    return result;
}
int main()
{
    {
        auto ledger = prepared();
        // Bought/free/old untracked bottles have no recorded expenditure.
        assert(!ledger.offer(1, 0x100ABC));
        assert(!ledger.offer(1, 0xFF001234));
        assert(!ledger.claim(1)); // Poison is active, but no lethal callback.
        assert(ledger.offer(1, a));
        assert(!ledger.offer(1, b)); // Actual lethal source cannot be replaced.
        assert(ledger.claim(1) == Ingredients({{0x100, 1}, {0x200, 1}}));
        for (ID victim = 1; victim <= 100; ++victim) {
            assert(!ledger.offer(victim, a)); assert(!ledger.claim(victim));
        }
        assert(ledger.eligible(b));
        assert(ledger.offer(200, b));
        assert(ledger.claim(200) == Ingredients({{0x100, 1}, {0x200, 2}}));
    }
    {
        auto ledger = prepared();
        assert(!ledger.offer(1, c));
        assert(ledger.offer(1, a)); assert(ledger.claim(1));
        assert(ledger.remember(flowers));
        assert(ledger.offer(2, c)); assert(ledger.claim(2));
        assert(ledger.remember(roots));
        assert(!ledger.offer(3, a)); // Reselecting cannot reset spent batches.
        assert(ledger.offer(3, b));
        ++ledger.sequence;
        assert(ledger.add({0xFF000200, 4, roots, {{0x100, 1}, {0x200, 1}}, false}));
        assert(ledger.offer(4, 0xFF000200)); assert(ledger.claim(4));
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, a)); assert(ledger.offer(11, a));
        assert(ledger.claim(11)); assert(!ledger.claim(10));
        ledger.forgetActor(11);
        assert(!ledger.offer(11, a));
    }
    {
        auto ledger = prepared();
        const auto ingredientExists = [](ID id) { return id == 0x100 || id == 0x200; };
        assert(ledger.unclaimed() == 2);
        // No proven lethal callback: being poisoned or a weapon kill cannot pay.
        assert(!ledger.claimVerified(10, ingredientExists));
        assert(ledger.offer(10, a));
        assert(ledger.offer(11, a));
        // Native hook has already proved death (kDying or kDead). Payout has
        // no actor lookup dependency and works on the very next queued task.
        assert(ledger.claimVerified(10, ingredientExists) == Ingredients({{0x100, 1}, {0x200, 1}}));
        assert(!ledger.claimVerified(11, ingredientExists));
        assert(!ledger.claimVerified(10, ingredientExists));
        assert(ledger.unclaimed() == 1);
        assert(ledger.candidates.empty());
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, a));
        // FEC or other corpse cleanup can delete the victim before the task.
        ledger.forgetActor(10);
        assert(ledger.candidates.contains(10));
        assert(ledger.claimVerified(10, [](ID) { return true; }));
        ledger.forgetActor(10);
        assert(!ledger.offer(10, a)); // Paid batch cannot be revived by deletion.
        assert(!ledger.claimVerified(10, [](ID) { return true; }));
        assert(ledger.unclaimed() == 1);
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, a));
        // Missing ingredients must not consume the batch or give a partial refund.
        assert(!ledger.claimVerified(10, [](ID id) { return id != 0x200; }));
        assert(ledger.unclaimed() == 2);
        assert(ledger.candidates.empty());
        assert(ledger.offer(11, a));
        assert(ledger.claimVerified(11, [](ID) { return true; }));
        assert(ledger.remember(flowers));
        assert(ledger.unclaimed() == 1);
        assert(ledger.offer(12, c));
        assert(ledger.remember(roots));
        assert(!ledger.claimVerified(12, [](ID) { return true; }));
    }
    {
        auto ledger = prepared();
        assert(ledger.offer(10, a));
        ledger.giftGiven = true; ledger.armed = true;
        const auto bytes = encode(ledger);
        const auto restore = decode(bytes, [](ID id) { return id; });
        assert(restore && encode(*restore) == bytes);
        assert(restore->giftGiven && restore->armed);
        auto paid = *restore; assert(paid.claimVerified(10, [](ID) { return true; }));
        auto again = decode(encode(paid), [](ID id) { return id; });
        assert(again && !again->offer(12, a)); assert(again->offer(12, b));
        auto remap = decode(bytes, [](ID id) { return id + 0x1000; });
        assert(remap && remap->stored == Recipe({0x1100, 0x1200}));
        assert(remap->claim(0x100A) == Ingredients({{0x1100, 1}, {0x1200, 1}}));
        auto missing = decode(bytes, [](ID id) { return id == 0x100 ? 0 : id; });
        assert(missing && !missing->eligible(a) && !missing->eligible(b));
        for (std::size_t i = 0; i < bytes.size(); ++i)
            assert(!decode(std::span(bytes).first(i), [](ID id) { return id; }));
        auto corrupt = bytes; corrupt.push_back(0);
        assert(!decode(corrupt, [](ID id) { return id; }));
        corrupt = bytes; corrupt[0] = 1;
        assert(!decode(corrupt, [](ID id) { return id; }));
    }
    {
        auto ledger = prepared();
        assert(!ledger.add({a, 1, roots, {{0x100, 1}, {0x200, 1}}, false}));
        assert(!ledger.add({0x123, 1, roots, {{0x100, 1}}, false}));
        assert(!ledger.add({0xFF002222, 3, roots, {}, false}));
        assert(!ledger.add({0xFF002222, 3, roots, {{0x100, 0}}, false}));
        assert(!ledger.add({0xFF002222, 3, roots, {{0x300, 1}}, false}));
        ++ledger.sequence;
        assert(ledger.add({0xFF002222, 4, roots, {{0x100, 1}}, false}));
        assert(ledger.offer(1, 0xFF002222));
        assert(ledger.claim(1) == Ingredients({{0x100, 1}}));
    }
    {
        // Two-ingredient and uneven recorded-cost batches cross the exact
        // learned-skill threshold without changing the saved expenditure.
        for (const auto level : {0.0f, 49.0f, 49.99f, 50.0f, 100.0f, 150.0f}) {
            auto ledger = prepared();
            const auto expectedSets = level < 50.0f ? 1u : 2u;
            assert(ledger.offer(10, a));
            assert(ledger.offer(11, a)); // Extra doses share this one allowance.
            assert(ledger.claimVerified(10, [](ID) { return true; }, level) ==
                Ingredients({{0x100, expectedSets}, {0x200, expectedSets}}));
            assert(!ledger.claimVerified(11, [](ID) { return true; }, level));
            assert(ledger.batches.at(a).cost == Ingredients({{0x100, 1}, {0x200, 1}}));
            assert(ledger.offer(20, b));
            assert(ledger.claimVerified(20, [](ID) { return true; }, level) ==
                Ingredients({{0x100, expectedSets}, {0x200, 2u * expectedSets}}));
            auto restored = decode(encode(ledger), [](ID id) { return id; });
            assert(restored && !restored->offer(30, a) && !restored->offer(30, b));
        }
    }
    {
        // Existing unclaimed batches upgrade at payout, including after a
        // save/reload. All three ingredients are returned twice, once only.
        auto ledger = prepared();
        assert(ledger.remember(flowers));
        assert(ledger.offer(10, c));
        auto restored = decode(encode(ledger), [](ID id) { return id; });
        assert(restored);
        assert(!restored->claimVerified(10, [](ID id) { return id != 0x400; }, 50.0f));
        assert(restored->eligible(c));
        assert(restored->offer(11, c));
        assert(restored->claimVerified(11, [](ID) { return true; }, 50.0f) ==
            Ingredients({{0x300, 2}, {0x400, 2}, {0x500, 2}}));
        assert(!restored->offer(12, c));
    }
    std::cout << "Huntsman's Satchel: batch, lethal-source, immediate refund, corpse deletion, recipe, multi-hit, save, cost and Alchemy 50 refund tests passed\n";
}
