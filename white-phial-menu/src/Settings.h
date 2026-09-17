#pragma once
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace phial
{
    enum Field : std::size_t { repaired, hours, hotkey, count };
    using Values = std::array<float, count>;
    inline constexpr std::array<std::string_view, count> editorIDs{
        "TWPTE_PhialIsFullyRepaired", "TWPTE_ResetHours", "TWPTE_HotkeyButton"
    };
    inline constexpr float minHours = 0.1f, maxHours = 8760.0f;

    inline bool equal(float a, float b) { return a == b; }
    inline bool valid(std::size_t field, float value)
    {
        if (!std::isfinite(value)) return false;
        switch (field) {
        case repaired: return value == 0 || value == 1;
        case hours: return value >= minHours && value <= maxHours;
        case hotkey: return value >= 1 && value <= 281 && std::floor(value) == value;
        default: return false;
        }
    }

    struct Request
    {
        std::uint64_t epoch{};
        std::uint8_t dirty{};
        Values expected{}, desired{};
        bool expectedRemember{}, remember{};
    };

    inline bool canApply(const Request& request, std::uint64_t epoch, const Values& current,
        bool remember = false, bool retrySave = false)
    {
        if (request.epoch != epoch || request.expectedRemember != remember || (request.dirty & ~7u)) return false;
        if (!request.dirty && request.remember == remember && !retrySave) return false;
        for (std::size_t i = 0; i < count; ++i) {
            if (!(request.dirty & (1u << i))) continue;
            if (!valid(i, request.desired[i])) return false;
            // Another mod/quest may have changed the value while this edit was pending.
            if (!equal(current[i], request.expected[i]) && !equal(current[i], request.desired[i])) return false;
        }
        if (request.remember) {
            for (std::size_t i = 0; i < count; ++i)
                if (!valid(i, (request.dirty & (1u << i)) ? request.desired[i] : current[i])) return false;
        }
        return true;
    }

    struct Draft
    {
        bool initialized{};
        Request request{};
        Values current{};

        void reset(std::uint64_t epoch, const Values& values, bool remember = false)
        {
            initialized = true;
            current = values;
            request = { epoch, 0, values, values, remember, remember };
        }

        void receive(std::uint64_t epoch, const Values& values, bool remember = false)
        {
            if (!initialized || request.epoch != epoch) { reset(epoch, values, remember); return; }
            current = values;
            if (request.remember == request.expectedRemember || request.remember == remember)
                request.remember = request.expectedRemember = remember;
            for (std::size_t i = 0; i < count; ++i) {
                const auto bit = static_cast<std::uint8_t>(1u << i);
                if (!(request.dirty & bit) || equal(values[i], request.desired[i])) {
                    request.desired[i] = request.expected[i] = values[i];
                    request.dirty &= static_cast<std::uint8_t>(~bit);
                }
            }
        }

        void edit(std::size_t field, float value)
        {
            if (field >= count) return;
            const auto bit = static_cast<std::uint8_t>(1u << field);
            if (!(request.dirty & bit)) request.expected[field] = current[field];
            request.desired[field] = value;
            if (equal(value, current[field])) request.dirty &= static_cast<std::uint8_t>(~bit);
            else request.dirty |= bit;
        }

        bool validEdits() const
        {
            if (!request.dirty && request.remember == request.expectedRemember) return false;
            for (std::size_t i = 0; i < count; ++i)
                if ((request.remember || (request.dirty & (1u << i))) && !valid(i, request.desired[i])) return false;
            return true;
        }
    };
}
