#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace stars {
struct Recipe {
    std::uint32_t starID;
    unsigned soul;
    std::uint32_t outputID;
    int count;
};
// Output IDs are local to RunemasterMagic.esl.
inline constexpr std::array<Recipe, 4> recipes{{
    {0x63B27, 3, 0x86E, 1},
    {0x63B27, 4, 0x86E, 2},
    {0x63B27, 5, 0x86E, 4},
    {0x63B29, 5, 0x86F, 4}
}};
constexpr bool eligible(unsigned index, std::uint32_t star, unsigned soul, int count) {
    return index < recipes.size() && star == recipes[index].starID &&
        soul == recipes[index].soul && count == 1;
}
enum class Result { crafted, unavailable, stale, deliveryFailed, rollbackFailed };

// Executed synchronously on the game thread. The Star is never removed or
// replaced. Its soul is cleared BEFORE delivery to prevent reentrant reuse.
template <class Inventory>
Result craft(Inventory& inventory, unsigned index) {
    if (index >= recipes.size() || !inventory.eligible(index)) return Result::unavailable;
    const auto& recipe = recipes[index];
    const int before = inventory.outputCount();
    if (before < 0 || before > std::numeric_limits<int>::max() - recipe.count) return Result::unavailable;
    if (!inventory.setSoul(recipe.soul, 0)) return Result::stale;
    inventory.give(recipe.count);
    if (inventory.outputCount() > before) return Result::crafted;
    return inventory.setSoul(0, recipe.soul) ? Result::deliveryFailed : Result::rollbackFailed;
}
constexpr bool currentTask(bool active, std::uint64_t queued, std::uint64_t current) {
    return active && queued == current;
}
}
