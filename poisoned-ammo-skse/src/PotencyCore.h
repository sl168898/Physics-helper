#pragma once
#include <cmath>

namespace potency
{
    // This is the skill level, not AlchemyModifier/AlchemyPowerModifier.
    inline float multiplier(float alchemy)
    {
        return std::isfinite(alchemy) && alchemy > 0 ? 1.0f + alchemy * 0.01f : 1.0f;
    }

    inline bool scaleDamage(float& magnitude, float alchemy)
    {
        const auto factor = multiplier(alchemy);
        if (factor <= 1 || !std::isfinite(magnitude) || magnitude == 0 ||
            !std::isfinite(magnitude * factor)) return false;
        magnitude *= factor;
        return true;
    }
}
