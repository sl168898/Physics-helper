#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace runes::crafting {
struct Identity {
    std::uint32_t enchantment{};
    std::uint16_t capacity{};
    float charge{};
    float health = 1.f;
    std::string name;
    bool temporary{};
    bool operator==(const Identity&) const = default;
};
struct Stack {
    Identity identity;
    std::int32_t count{};
};
inline void append(std::vector<Stack>& stacks, Identity value, std::int32_t count) {
    if (count <= 0) return;
    for (auto& stack : stacks) {
        if (stack.identity == value) { stack.count += count; return; }
    }
    stacks.push_back({std::move(value), count});
}
// Determine the actual lost variant, even when the engine selects a different
// inventory copy from the first one. Refuse all ambiguous/multi-item changes.
inline std::optional<Identity> singleLoss(const std::vector<Stack>& before, const std::vector<Stack>& after) {
    std::optional<Identity> result;
    std::int64_t lost = 0;
    for (const auto& old : before) {
        std::int64_t remaining = 0;
        for (const auto& now : after) if (now.identity == old.identity) remaining += now.count;
        const auto delta = static_cast<std::int64_t>(old.count) - remaining;
        if (delta < 0) return std::nullopt;
        if (delta) { lost += delta; result = old.identity; }
    }
    for (const auto& now : after) {
        bool existed = false;
        for (const auto& old : before) if (old.identity == now.identity) { existed = true; break; }
        if (!existed && now.count) return std::nullopt;
    }
    return lost == 1 ? result : std::nullopt;
}
inline bool matched(std::size_t consumed, std::size_t outputs, std::size_t signals,
    bool allOutputsIdentified, std::size_t plainOutputs, std::int32_t currentPlain, std::int32_t lastPlainAfter) {
    return consumed > 0 && consumed == outputs && outputs == signals && allOutputsIdentified &&
        lastPlainAfter >= static_cast<std::int32_t>(plainOutputs) && currentPlain >= lastPlainAfter;
}
inline bool transferable(const Identity& value) {
    return value.enchantment != 0 && !value.temporary && std::isfinite(value.charge) &&
        value.charge >= 0.f && std::isfinite(value.health) && value.health > 0.f;
}
}
