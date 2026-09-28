#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace harvest
{
    using NameCounts = std::map<std::string, std::int64_t>;
    constexpr std::size_t maxBatchNameBytes = 4096;

    inline bool validBatchName(const std::string& name)
    {
        return name.size() <= maxBatchNameBytes && name.find('\0') == std::string::npos;
    }

    inline std::int64_t nameCount(const NameCounts& counts, const std::string& name)
    {
        const auto it = counts.find(name);
        return it == counts.end() ? 0 : it->second;
    }

    // The empty string denotes copies without a custom display name.
    // Require one positive named delta and no disappearing/renamed old batch.
    inline std::optional<std::string> craftedBatchName(
        const NameCounts& before, const NameCounts& after, std::int64_t bottles)
    {
        if (bottles <= 0) return std::nullopt;
        for (const auto& [name, count] : before)
            if (count < 0 || nameCount(after, name) < count) return std::nullopt;
        std::optional<std::string> result;
        for (const auto& [name, count] : after) {
            if (!validBatchName(name) || count < 0) return std::nullopt;
            const auto delta = count - nameCount(before, name);
            if (delta == 0) continue;
            if (delta != bottles || result) return std::nullopt;
            result = name;
        }
        return result;
    }
}
