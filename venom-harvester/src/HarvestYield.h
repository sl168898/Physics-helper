#pragma once
#include "Harvest.h"
#include <cmath>

namespace harvest
{
    // Skyrim PERK entry point 87: Mod Ingredients Harvested. These are
    // constant, one-value entries; crafting yield (89) does not qualify.
    inline bool doublesHarvestYield(std::uint32_t entryPoint,
        std::uint32_t function, float value)
    {
        if (entryPoint != 87 || !std::isfinite(value)) return false;
        switch (function) {
        case 1: // Set Value
        case 3: // Multiply Value
            return value >= 2.0f;
        case 2: // Add Value, relative to the ordinary one-set yield
            return value >= 1.0f;
        default:
            return false;
        }
    }

    template<class ValidIngredient>
    std::optional<Ingredients> claimRefund(Ledger& ledger, ID victim,
        bool doubleYield, ValidIngredient validIngredient)
    {
        // Claim the single allowance first. Scale only the returned copy;
        // saved expenditure and paid state must never be multiplied/reset.
        auto reward = ledger.claimVerified(victim, validIngredient);
        if (reward && doubleYield)
            for (auto& part : *reward) part.count *= 2;
        // Ledger validation limits each cost to 100000, so 2x fits int32.
        return reward;
    }
}
