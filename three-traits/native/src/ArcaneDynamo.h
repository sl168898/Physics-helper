#pragma once
#include <cmath>
#include <cstdint>
#include <limits>

namespace traits::dynamo {
inline constexpr float baseCost = 15.f;
inline constexpr float damageMultiplier = 1.30f;
inline constexpr float regenerationMultiplier = 0.65f;

// Normalize the engine's final charge cost against the same enchantment's
// actor-independent cost. Modifiers still matter, without making a more
// expensive enchantment arbitrarily cost more than the proposed 15 Magicka.
inline float cost(float unmodified, float modified) {
    if (!std::isfinite(unmodified) || !std::isfinite(modified) || unmodified < 0.f)
        return std::numeric_limits<float>::infinity();
    if (modified <= 0.f) return 0.f;
    if (unmodified == 0.f) return std::numeric_limits<float>::infinity();
    const double result = static_cast<double>(baseCost) * modified / unmodified;
    return result <= std::numeric_limits<float>::max() ? static_cast<float>(result)
        : std::numeric_limits<float>::infinity();
}
inline bool canPay(float magicka, float amount) {
    return std::isfinite(amount) && amount >= 0.f && std::isfinite(magicka) &&
           (amount == 0.f || magicka >= amount);
}
inline bool resource(std::int32_t av) { return av >= 24 && av <= 26; }
inline bool damageEffect(std::int32_t archetype, bool detrimental, bool recover,
    bool noMagnitude, std::int32_t primary, std::int32_t secondary) {
    return detrimental && !recover && !noMagnitude &&
        ((archetype == 0 || archetype == 4) ? resource(primary) :
         archetype == 5 && (resource(primary) || resource(secondary)));
}
inline float damage(float magnitude, bool eligible) {
    if (!eligible || !std::isfinite(magnitude) || magnitude <= 0.f ||
        magnitude > std::numeric_limits<float>::max() / damageMultiplier) return magnitude;
    return magnitude * damageMultiplier;
}
inline float regeneration(float rate, bool selected) {
    return selected && std::isfinite(rate) && rate > 0.f ? rate * regenerationMultiplier : rate;
}

template<class T> class Scope {
    T& slot;
    T previous;
public:
    Scope(T& slot, T next) : slot(slot), previous(slot) { slot = next; }
    ~Scope() { slot = previous; }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
