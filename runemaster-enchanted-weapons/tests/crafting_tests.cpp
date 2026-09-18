#include "CraftingRules.h"
#include <cassert>
#include <limits>

int main() {
    using namespace runes::crafting;
    Identity plain;
    Identity fire{0xFF000ABC, 2000, 731.5f, 1.4f, "Ember", false};
    Identity frost{0xFF000ABD, 1000, 0.f, 1.f, "Rime", false};
    std::vector<Stack> before{{fire, 1}, {frost, 1}, {plain, 3}};
    // The consumed item is the depleted frost weapon, not the first enchantment
    // returned by the inventory iterator. Zero charge must remain zero.
    auto lost = singleLoss(before, {{plain, 3}, {fire, 1}});
    assert(lost && *lost == frost && lost->charge == 0.f);
    lost = singleLoss(before, {{fire, 1}, {frost, 1}, {plain, 2}});
    assert(lost && *lost == plain && !transferable(*lost));
    lost = singleLoss({{fire, 3}}, {{fire, 2}});
    assert(lost && *lost == fire && lost->charge == 731.5f && lost->capacity == 2000);
    assert(lost->health == 1.4f && lost->name == "Ember");
    assert(singleLoss({{fire, 1}}, {}));
    assert(!singleLoss(before, before));
    assert(!singleLoss(before, {{plain, 3}})); // Two different weapons removed.
    assert(!singleLoss({{fire, 3}}, {{fire, 1}})); // Two copies removed.
    assert(!singleLoss({{fire, 1}}, {{frost, 1}})); // Replacement, not consumption.
    assert(!singleLoss({{fire, 1}}, {{fire, 2}}));
    auto changed = fire; changed.charge = 123.f;
    assert(!singleLoss({{fire, 1}}, {{changed, 1}}));
    assert(transferable(fire) && transferable(frost));
    changed = fire; changed.temporary = true; assert(!transferable(changed));
    changed = fire; changed.charge = -1.f; assert(!transferable(changed));
    changed = fire; changed.charge = std::numeric_limits<float>::quiet_NaN(); assert(!transferable(changed));
    std::vector<Stack> aggregate;
    append(aggregate, fire, 1); append(aggregate, fire, 2); append(aggregate, frost, 1);
    assert(aggregate.size() == 2 && aggregate[0].count == 3);
    // Require source consumption, actual output addition, AND a craft event.
    // Existing identical plain copies do not prevent preserving the new item.
    assert(matched(1, 1, 1, true, 1, 8, 8));
    assert(matched(2, 2, 2, true, 2, 9, 9));
    assert(matched(1, 1, 1, true, 0, 0, 0)); // New individually tracked output.
    assert(!matched(0, 1, 1, true, 1, 8, 8));
    assert(!matched(1, 0, 1, true, 0, 8, 8));
    assert(!matched(1, 1, 0, true, 1, 8, 8));
    assert(!matched(1, 1, 1, false, 1, 8, 8));
    assert(!matched(1, 1, 1, true, 1, 7, 8)); // New output already removed/changed.
    assert(!matched(2, 1, 1, true, 1, 8, 8));
    assert(!matched(2, 2, 1, true, 2, 8, 8));
}
