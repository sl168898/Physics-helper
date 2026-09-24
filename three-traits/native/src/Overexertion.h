#pragma once
#include <algorithm>
#include <cmath>

namespace traits {
// One gameplay-time window. A new manual release refreshes, never stacks.
struct Overexertion {
    float remaining = 0;
    void shout() { remaining = 3.f; }
    void clear() { remaining = 0; }
    void tick(float dt, bool active) {
        if (!active) { clear(); return; }
        if (std::isfinite(dt) && dt > 0) remaining = std::max(0.f, remaining - dt);
    }
    float extraDamage(float total, float physical) const {
        if (remaining <= 0 || !std::isfinite(total) || !std::isfinite(physical) || total <= 0 || physical <= 0) return 0;
        // Preserve any nonphysical remainder and never create damage from a
        // fully blocked hit. Resisted damage is not damage received.
        return 0.20f * std::min(physical, total);
    }
};
}
