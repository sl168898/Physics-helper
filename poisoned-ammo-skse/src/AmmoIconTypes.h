// MIT; Physics-helper contributors, 2026-09-30.
// Shared, pointer-free icon metadata contract. No gameplay or save-format data.
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace AmmoIcon
{
    enum class Damage : std::uint8_t { None, Fire, Frost, Shock, Poison };
    constexpr bool Valid(Damage value) { return static_cast<unsigned>(value) <= 4; }
    constexpr std::string_view Name(Damage value)
    {
        constexpr std::array<std::string_view, 5> names{ "none", "fire", "frost", "shock", "poison" };
        return Valid(value) ? names[static_cast<unsigned>(value)] : names[0];
    }
    struct State
    {
        bool bolt{};
        Damage native = Damage::None;
        Damage coating = Damage::None;
        constexpr Damage Head() const { return native == Damage::None ? coating : native; }
        constexpr bool NeedsNativeIcon() const { return bolt || native != Damage::None || coating != Damage::None; }
        constexpr std::uint32_t Key() const
        {
            return (bolt ? 0x100u : 0u) | static_cast<std::uint32_t>(native) |
                (static_cast<std::uint32_t>(coating) << 4);
        }
        std::string Filename() const
        {
            return std::string(bolt ? "bolt_" : "arrow_") + std::string(Name(Head())) + "_" +
                std::string(Name(coating)) + ".svg";
        }
    };
    // GetIconInfoV1 returns zero for unavailable/not-owned ammo; otherwise this
    // tagged word. Bits 0..3: native type, 4..7: coating, 8: bolt. No pointers.
    constexpr std::uint32_t apiTag = 0xA1010000u;
    constexpr std::uint32_t PackCoated(State state)
    {
        return Valid(state.native) && Valid(state.coating) && state.coating != Damage::None ?
            apiTag | state.Key() : 0u;
    }
    constexpr std::optional<State> UnpackCoated(std::uint32_t word)
    {
        if ((word & ~0x1FFu) != apiTag) return std::nullopt;
        State state{ (word & 0x100u) != 0, static_cast<Damage>(word & 15u),
            static_cast<Damage>((word >> 4) & 15u) };
        if (!Valid(state.native) || !Valid(state.coating) || state.coating == Damage::None) return std::nullopt;
        return state;
    }
    struct Scores
    {
        std::array<double, 5> values{};
        void Add(Damage type, float magnitude, std::uint32_t duration, bool noMagnitude, bool noDuration)
        {
            if (!Valid(type) || type == Damage::None || !std::isfinite(magnitude)) return;
            const double strength = noMagnitude ? 1.0 : std::fabs(static_cast<double>(magnitude));
            if (strength <= 0) return;
            const double time = noDuration || duration == 0 ? 1.0 : static_cast<double>(duration);
            values[static_cast<unsigned>(type)] += strength * time;
        }
        Damage Dominant(Damage fallback = Damage::None) const
        {
            double best = 0;
            Damage result = fallback;
            // Equal scores use a stable Fire/Frost/Shock/Poison order.
            for (unsigned i = 1; i < values.size(); ++i) {
                if (std::isfinite(values[i]) && values[i] > best) {
                    best = values[i]; result = static_cast<Damage>(i);
                }
            }
            return result;
        }
    };
}
