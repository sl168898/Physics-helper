#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace ovation
{
    // Stable on-disk order and matching ESP forms. Speech variants are separate
    // actor values; do not silently convert Fortify Barter to Fortify Speech.
    inline constexpr std::array<std::uint32_t, 6> actorValues{24, 25, 26, 17, 107, 146};
    inline constexpr std::uint32_t spellStart = 0x870;
    inline constexpr std::uint32_t effectStart = 0x860;
    using Amounts = std::array<float, actorValues.size()>;

    inline std::optional<std::size_t> slot(std::uint32_t av)
    {
        for (std::size_t i = 0; i < actorValues.size(); ++i)
            if (actorValues[i] == av) return i;
        return std::nullopt;
    }

    inline bool addTriple(Amounts& amounts, std::uint32_t av, float base)
    {
        const auto index = slot(av);
        if (!index || !std::isfinite(base) || base < 0.0f) return false;
        const auto result = amounts[*index] + base * 3.0f;
        if (!std::isfinite(result)) return false;
        amounts[*index] = result;
        return true;
    }

    inline Amounts mergeReward(Amounts existing, const Amounts& awarded, std::uint32_t replaced)
    {
        for (std::size_t i = 0; i < existing.size(); ++i)
            if (replaced & (1u << i)) existing[i] = awarded[i];
        return existing;
    }

    struct State
    {
        std::uint32_t active{};
        std::uint32_t lastTown{};
        Amounts amounts{};

        bool valid() const
        {
            if (active > 1) return false;
            bool positive = false;
            for (const auto value : amounts) {
                if (!std::isfinite(value) || value < 0.0f) return false;
                positive = positive || value > 0.0f;
            }
            return active == 0 || positive;
        }

        void award(const Amounts& values, std::uint32_t town)
        {
            active = 1;
            amounts = values; // A repeat performance replaces the reward.
            lastTown = town;
        }

        bool enter(std::uint32_t town)
        {
            const bool expires = active && town && town != lastTown;
            lastTown = town;
            if (expires) {
                active = 0;
                amounts = {};
            }
            return expires;
        }
    };
    static_assert(sizeof(State) == 32);
}
