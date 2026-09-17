#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

namespace waterskin
{
    struct Counts {
        std::int64_t filled = 0;
        std::int64_t water = 0;
        std::int64_t empty = 0;
    };

    // Fail closed unless the original craft performed the expected transaction.
    // Never award from inventory additions alone, and never award twice if a
    // different native patch already supplied the byproduct inside this call.
    constexpr std::int32_t reward(bool targetRecipe, Counts before, Counts after)
    {
        if (!targetRecipe || before.filled < 0 || before.water < 0 || before.empty < 0 ||
            after.filled < 0 || after.water < 0 || after.empty < 0) return 0;
        const auto consumed = before.filled - after.filled;
        if (consumed <= 0 || consumed > std::numeric_limits<std::int32_t>::max()) return 0;
        if (after.water - before.water != consumed * 3) return 0;
        const auto alreadyReturned = std::max<std::int64_t>(0, after.empty - before.empty);
        return static_cast<std::int32_t>(std::max<std::int64_t>(0, consumed - alreadyReturned));
    }
}
