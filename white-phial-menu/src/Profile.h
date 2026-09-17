#pragma once
#include "Settings.h"
#include <sstream>

namespace phial
{
    struct Profile
    {
        bool enabled{};
        Values values{};
    };

    struct ProfileResult
    {
        Profile profile{};
        std::string error;
        explicit operator bool() const { return error.empty(); }
    };

    inline bool validProfile(const Profile& profile)
    {
        if (!profile.enabled) return true;
        for (std::size_t i = 0; i < count; ++i) if (!valid(i, profile.values[i])) return false;
        return true;
    }

    inline std::string_view trim(std::string_view text)
    {
        constexpr auto spaces = " \t\r\n";
        const auto first = text.find_first_not_of(spaces);
        if (first == text.npos) return {};
        return text.substr(first, text.find_last_not_of(spaces) - first + 1);
    }

    inline ProfileResult parseProfile(std::string_view contents)
    {
        if (contents.size() > 16384) return { {}, "Configuration exceeds 16 KiB." };
        if (contents.starts_with("\xEF\xBB\xBF")) contents.remove_prefix(3);
        Profile result;
        std::array<bool, 4> seen{};
        bool section = false;
        std::istringstream input{ std::string(contents) };
        std::string raw;
        constexpr std::array<std::string_view, 4> names{
            "RememberAcrossSaves", "FullyReenchanted", "RefillHours", "Hotkey"
        };
        while (std::getline(input, raw)) {
            auto line = trim(raw);
            if (line.empty() || line.front() == ';' || line.front() == '#') continue;
            if (line.front() == '[') { section = line == "[WhitePhial]"; continue; }
            if (!section) continue;
            const auto split = line.find('=');
            if (split == line.npos) return { {}, "Expected key=value in WhitePhial section." };
            const auto name = trim(line.substr(0, split));
            auto valueText = line.substr(split + 1);
            valueText = trim(valueText.substr(0, valueText.find_first_of(";#")));
            for (std::size_t i = 0; i < names.size(); ++i) {
                if (name != names[i]) continue;
                if (seen[i]) return { {}, "Duplicate configuration key: " + std::string(name) };
                seen[i] = true;
                if (valueText.empty()) return { {}, "Missing value for " + std::string(name) };
                float value{};
                const auto [end, error] = std::from_chars(valueText.data(), valueText.data() + valueText.size(), value);
                if (error != std::errc{} || end != valueText.data() + valueText.size() || !std::isfinite(value))
                    return { {}, "Invalid numeric value for " + std::string(name) };
                if (i == 0) {
                    if (value != 0 && value != 1) return { {}, "RememberAcrossSaves must be 0 or 1." };
                    result.enabled = value == 1;
                } else {
                    if (!valid(i - 1, value)) return { {}, "Out-of-range value for " + std::string(name) };
                    result.values[i - 1] = value;
                }
            }
        }
        if (!seen[0]) return { {}, "RememberAcrossSaves is missing." };
        if (result.enabled && (!seen[1] || !seen[2] || !seen[3]))
            return { {}, "Shared settings require enchantment, refill time and hotkey values." };
        return { result, {} };
    }

    inline std::optional<std::string> serializeProfile(const Profile& profile)
    {
        if (!validProfile(profile)) return std::nullopt;
        std::string text = "; Managed by White Phial Menu. Configure through White Phial > Settings.\n[WhitePhial]\nRememberAcrossSaves=";
        text += profile.enabled ? "1\n" : "0\n";
        if (!profile.enabled) return text;
        constexpr std::array<std::string_view, count> names{ "FullyReenchanted", "RefillHours", "Hotkey" };
        for (std::size_t i = 0; i < count; ++i) {
            char number[64];
            const auto [end, error] = std::to_chars(number, number + sizeof(number), profile.values[i], std::chars_format::fixed);
            if (error != std::errc{}) return std::nullopt;
            text += std::string(names[i]) + "=" + std::string(number, end) + "\n";
        }
        return text;
    }
}
