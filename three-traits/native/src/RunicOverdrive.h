#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace traits {
enum class RunicScaling { none, magnitude, duration };

// Archetype numbers are the native Skyrim enum; the adapter asserts the
// representative values against the pinned CommonLib headers at compile time.
inline RunicScaling runicScaling(std::int32_t archetype, bool noMagnitude,
                                 bool noDuration, float magnitude, float duration) {
    // A script's timer, summon count, proc chance or explosion cannot safely
    // be inferred from its carrier's numbers. Only scale defined native values.
    if (archetype == 11 || archetype == 21 || archetype == 23 || archetype == 41) {
        return !noDuration && std::isfinite(duration) && duration > 0 &&
               duration <= std::numeric_limits<float>::max() / 2
            ? RunicScaling::duration : RunicScaling::none;
    }
    switch (archetype) {
    case 0: case 2: case 4: case 5: case 6: case 7: case 8: case 9:
    case 10: case 22: case 24: case 31: case 32: case 34: case 38: case 42:
        return !noMagnitude && std::isfinite(magnitude) && magnitude != 0 &&
               std::abs(magnitude) <= std::numeric_limits<float>::max() / 2
            ? RunicScaling::magnitude : RunicScaling::none;
    default: return RunicScaling::none;
    }
}

struct RunicHit {
    bool present = false, eligible = false;
    std::uint32_t attacker = 0, target = 0, weapon = 0;
    bool matches(std::uint32_t a, std::uint32_t t, std::uint32_t w) const {
        return present && eligible && attacker != 0 && target != 0 && weapon != 0 &&
               attacker == a && target == t && (w == 0 || weapon == w);
    }
};
class RunicHitScope {
    RunicHit& slot;
    RunicHit previous;
public:
    RunicHitScope(RunicHit& slot, RunicHit next) : slot(slot), previous(slot) { slot = next; }
    ~RunicHitScope() { slot = previous; }
    RunicHitScope(const RunicHitScope&) = delete;
    RunicHitScope& operator=(const RunicHitScope&) = delete;
};

inline bool runicEligible(bool selected, bool player, bool weaponEnchantment,
                          bool contact, bool power, bool bash, bool melee) {
    return selected && player && weaponEnchantment && contact && power && !bash && melee;
}
}
