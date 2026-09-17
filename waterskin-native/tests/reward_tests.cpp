#include "RewardRule.h"
#include <cassert>
#include <iostream>

int main()
{
    using waterskin::Counts;
    using waterskin::reward;
    assert(reward(true, {5, 10, 2}, {4, 13, 2}) == 1);
    assert(reward(true, {5, 10, 2}, {2, 19, 2}) == 3);
    assert(reward(true, {5, 10, 2}, {5, 10, 2}) == 0); // Cancelled
    assert(reward(false, {5, 10, 2}, {4, 13, 2}) == 0); // Different recipe
    assert(reward(true, {5, 10, 2}, {5, 13, 2}) == 0); // Water gained alone
    assert(reward(true, {5, 10, 2}, {4, 10, 2}) == 0); // Skin consumed alone
    assert(reward(true, {5, 10, 2}, {4, 11, 2}) == 0); // Wrong yield
    assert(reward(true, {5, 10, 2}, {4, 14, 2}) == 0); // Unexpected yield
    assert(reward(true, {5, 10, 2}, {4, 13, 3}) == 0); // Already returned
    assert(reward(true, {5, 10, 2}, {2, 19, 3}) == 2); // Partial existing return
    assert(reward(true, {5, 10, 2}, {2, 19, 8}) == 0); // Never negative
    assert(reward(true, {5, 10, 2}, {4, 13, 1}) == 1); // Don't replace other lost skins
    assert(reward(true, {0, 0, 0}, {-1, 3, 0}) == 0);
    assert(reward(true, {2147483648LL, 0, 0}, {0, 6442450944LL, 0}) == 0);
    // Repeated crafts produce exactly one return per confirmed transaction.
    Counts now{100, 0, 0};
    for (int i = 0; i < 100; ++i) {
        auto after = now; --after.filled; after.water += 3;
        after.empty += reward(true, now, after); now = after;
    }
    assert(now.filled == 0 && now.water == 300 && now.empty == 100);
    std::cout << "PASS: success, batches, cancellation, wrong recipe/yield, no duplicate return, repeated crafts, bounds\n";
}
