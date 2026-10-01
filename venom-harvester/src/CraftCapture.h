#pragma once
#include "Harvest.h"

namespace harvest
{
    using InventoryCounts = std::map<ID, std::int32_t>;

    inline std::int64_t inventoryCount(const InventoryCounts& counts, ID item)
    {
        const auto it = counts.find(item);
        return it == counts.end() ? 0 : it->second;
    }

    enum class OutputStatus { found, noCraftEvent, noOutput, ambiguous, invalidCount };
    struct CraftOutput {
        OutputStatus status = OutputStatus::noOutput;
        ID item{};
        std::int32_t bottles{};
    };
    inline const char* outputStatusName(OutputStatus status)
    {
        switch (status) {
        case OutputStatus::found: return "found";
        case OutputStatus::noCraftEvent: return "no captured poison craft event";
        case OutputStatus::noOutput: return "no new unbound poison in inventory";
        case OutputStatus::ambiguous: return "multiple new poison forms; output is ambiguous";
        case OutputStatus::invalidCount: return "invalid poison inventory count";
        }
        return "unknown";
    }

    // ItemCrafted can report DefaultPoison (0005629E), not the finished
    // engine-created potion. It is evidence of a craft, not output identity.
    // Resolve the one unbound poison actually added within that callback.
    template<class IsUnboundPoison>
    CraftOutput identifyCraftOutput(bool poisonCrafted, const InventoryCounts& before,
        const InventoryCounts& after, IsUnboundPoison isUnboundPoison)
    {
        if (!poisonCrafted) return {OutputStatus::noCraftEvent};
        CraftOutput result;
        for (const auto& [item, count] : after) {
            const auto oldCount = inventoryCount(before, item);
            const auto added = static_cast<std::int64_t>(count) - oldCount;
            if (added <= 0 || !isUnboundPoison(item)) continue;
            if (oldCount < 0 || count < 0 || added > 100000)
                return {OutputStatus::invalidCount};
            if (result.status == OutputStatus::found)
                return {OutputStatus::ambiguous};
            result = {OutputStatus::found, item, static_cast<std::int32_t>(added)};
        }
        return result;
    }

    inline std::optional<Ingredients> consumedIngredients(const Recipe& recipe,
        const InventoryCounts& before, const InventoryCounts& after)
    {
        if (!validRecipe(recipe)) return std::nullopt;
        Ingredients cost;
        for (auto ingredient : recipe) {
            const auto oldCount = inventoryCount(before, ingredient);
            const auto newCount = inventoryCount(after, ingredient);
            const auto consumed = oldCount - newCount;
            if (oldCount < 0 || newCount < 0 || consumed < 0 || consumed > 100000)
                return std::nullopt;
            if (consumed) cost.push_back({ingredient, static_cast<std::uint32_t>(consumed)});
        }
        return cost; // Empty means a free craft: rememberable, no refund budget.
    }
}
