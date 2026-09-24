#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace traits {
struct Skald {
    std::uint32_t shout = 0;
    float remaining = 0;
    static float cooldown(float speech) {
        return std::isfinite(speech) && speech >= 100 ? 3.f :
            std::isfinite(speech) && speech >= 50 ? 6.f : 10.f;
    }
    void tick(float dt) {
        if (std::isfinite(dt) && dt > 0) remaining = std::max(0.f, remaining - dt);
    }
    bool start(float speech, bool selected, bool usable, bool bash, bool power, bool melee) {
        if (!selected || !usable || !shout || bash || !power || !melee || remaining > 0) return false;
        remaining = cooldown(speech);
        return true;
    }
    // Choosing/clearing a shout does not reset recovery. The same is true of
    // removing and reselecting the trait, so its menu cannot bypass recovery.
    void choose(std::uint32_t id) { shout = id; }
    void restore(std::uint32_t id, float seconds) {
        shout = id;
        remaining = std::isfinite(seconds) ? std::clamp(seconds, 0.f, 10.f) : 10.f;
    }
};
}
