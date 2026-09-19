#include "CraftingRules.h"
#include <cassert>
#include <iostream>

struct Inventory {
    std::uint32_t star{0x63B27};
    unsigned soul{5}, identity{12345}, otherExtra{6789};
    int instanceCount{1}, stones{}, gifts{}, mutations{};
    bool deliver{true}, stale{}, reenter{}, removeInstanceOnDelivery{};
    bool eligible(unsigned i) const { return stars::eligible(i, star, soul, instanceCount); }
    int outputCount() const { return stones; }
    bool setSoul(unsigned expected, unsigned value) {
        if (stale || !instanceCount || soul != expected) return false;
        soul = value; ++mutations; return true;
    }
    void give(int count) {
        ++gifts;
        if (reenter) {
            reenter = false;
            for (unsigned i = 0; i < 4; ++i) assert(stars::craft(*this, i) == stars::Result::unavailable);
        }
        if (deliver) stones += count;
        if (removeInstanceOnDelivery) instanceCount = 0;
    }
};
int main() {
    using stars::Result;
    unsigned cases = 0;
    // Exhaust all Star/soul combinations against every recipe. This catches
    // empty Stars, smaller souls, wrong Star types, and wrong-tier yields.
    for (auto star : {0x63B27u, 0x63B29u, 0x2E4FFu}) for (unsigned soul = 0; soul <= 6; ++soul) {
        for (unsigned i = 0; i < 4; ++i) {
            Inventory inv; inv.star = star; inv.soul = soul; inv.stones = 7;
            const bool expected = (star == 0x63B27 && ((i == 0 && soul == 3) || (i == 1 && soul == 4) || (i == 2 && soul == 5))) ||
                (star == 0x63B29 && i == 3 && soul == 5);
            const auto result = stars::craft(inv, i);
            if (expected) {
                const int amounts[] = {1,2,4,4};
                assert(result == Result::crafted && inv.soul == 0 && inv.stones == 7 + amounts[i] && inv.gifts == 1);
                assert(stars::craft(inv, i) == Result::unavailable && inv.gifts == 1);
            } else assert(result == Result::unavailable && inv.soul == soul && inv.stones == 7 && inv.gifts == 0);
            assert(inv.identity == 12345 && inv.otherExtra == 6789 && inv.instanceCount == 1);
            ++cases;
        }
    }
    {
        Inventory inv; inv.deliver = false;
        assert(stars::craft(inv, 2) == Result::deliveryFailed && inv.soul == 5 && inv.stones == 0 && inv.mutations == 2); ++cases;
    }
    {
        Inventory inv; inv.stale = true;
        assert(stars::craft(inv, 2) == Result::stale && inv.soul == 5 && inv.gifts == 0); ++cases;
    }
    {
        Inventory inv; inv.reenter = true;
        assert(stars::craft(inv, 2) == Result::crafted && inv.stones == 4 && inv.gifts == 1); ++cases;
    }
    {
        Inventory inv; inv.stones = std::numeric_limits<int>::max() - 2;
        assert(stars::craft(inv, 2) == Result::unavailable && inv.soul == 5 && inv.gifts == 0); ++cases;
    }
    {
        Inventory inv; inv.deliver = false; inv.removeInstanceOnDelivery = true;
        assert(stars::craft(inv, 2) == Result::rollbackFailed); ++cases;
    }
    for (int count : {0,2,-1}) {
        Inventory inv; inv.instanceCount = count;
        assert(stars::craft(inv, 2) == Result::unavailable && inv.soul == 5 && inv.gifts == 0); ++cases;
    }
    // Save/load cancellation of pending UI refreshes.
    assert(!stars::currentTask(false, 1, 1));
    assert(!stars::currentTask(true, 1, 2));
    assert(stars::currentTask(true, 2, 2));
    std::cout << cases << " crafting scenarios passed; save/load guards passed\n";
}
