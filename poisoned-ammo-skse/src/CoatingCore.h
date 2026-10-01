#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace coating
{
    // Rank II replaces rank I's multiplier. Quantity and strength are independent.
    inline float strength(bool rank1, bool rank2) { return rank2 ? 1.5f : rank1 ? 1.25f : 1.0f; }
    inline std::uint32_t doseCount(std::uint32_t base, bool bolts, bool measured)
    {
        base = std::clamp(base, 1u, 10000u);
        return base * (bolts && measured ? 2u : 1u);
    }
    inline bool scale(float& magnitude, float& duration, bool noMagnitude, float multiplier)
    {
        auto& value = noMagnitude ? duration : magnitude;
        if (multiplier <= 1.0f || !std::isfinite(value) || value == 0 ||
            (noMagnitude && value < 0) || !std::isfinite(value * multiplier)) return false;
        value *= multiplier;
        return true;
    }
}
