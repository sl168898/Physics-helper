#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>

namespace traits {
struct IronLungsQuote {
    float staminaBefore = 0, cost = 0, bonus = 0;
    bool allowed = false;
};

inline IronLungsQuote quoteIronLungs(float current, float maximum, float recoveryMultiplier) {
    IronLungsQuote q;
    if (!std::isfinite(current) || !std::isfinite(maximum) ||
        !std::isfinite(recoveryMultiplier) || current < 0 || maximum <= 0) return q;
    q.staminaBefore = current;
    q.cost = maximum * std::max(0.10f, 0.25f * recoveryMultiplier);
    q.bonus = 0.25f * current;
    // One float ULP covers e.g. 400 * 0.25 * 1.2 = 120.000008.
    // Accept exactly 120 displayed/actual Stamina, without overdrawing it.
    q.allowed = std::isfinite(q.cost) && std::isfinite(q.bonus) && q.cost > 0 &&
        q.cost <= std::nextafter(current, std::numeric_limits<float>::infinity());
    if (q.allowed) q.cost = std::min(q.cost, current);
    return q;
}

struct IronLungsCast {
    IronLungsQuote quote;
    std::uint64_t serial = 0;
    unsigned projectiles = 0;
    std::unordered_set<std::uint32_t> targets;
    bool claimTarget(std::uint32_t id) { return id && targets.insert(id).second; }
};
}
