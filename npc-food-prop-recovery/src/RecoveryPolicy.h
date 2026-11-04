#pragma once
#include <cstdint>
#include <optional>

namespace foodprop {
enum class Observation { Healthy, Missing, Hidden, Pending, Suppressed, Ambiguous };
enum class Action { None, ReloadAndDraw, DrawOnly };

// Times are milliseconds of active gameplay, not wall time spent in menus.
struct Policy {
    bool drawn = false;
    bool awaitingResult = false;
    std::uint32_t attempts = 0;
    std::uint64_t born = 0, lastAttempt = 0;
    std::optional<std::uint64_t> missingSince;

    explicit Policy(std::uint64_t now = 0) : born(now) {}
    void onDraw() { drawn = true; missingSince.reset(); }
    void onHide() { drawn = false; awaitingResult = false; missingSince.reset(); }
    bool expired(std::uint64_t now) const { return now < born || now - born > 120000; }

    Action observe(Observation obs, std::uint64_t now, bool eligible, bool repairs) {
        if (!drawn || !eligible || expired(now)) {
            missingSince.reset();
            return Action::None;
        }
        if (obs == Observation::Healthy) {
            awaitingResult = false;
            missingSince.reset();
            return Action::None;
        }
        // In-flight asynchronous loads, deliberate hiding and conflicting
        // registry entries must never be "fixed" by spawning another prop.
        if (obs == Observation::Pending || obs == Observation::Suppressed ||
            obs == Observation::Ambiguous) {
            missingSince.reset();
            return Action::None;
        }
        if (!missingSince) missingSince = now;
        if (!repairs || attempts >= 2 || now - *missingSince < 1500 ||
            now - born < 1500 || (attempts && now - lastAttempt < 10000)) {
            return Action::None;
        }
        ++attempts;
        lastAttempt = now;
        awaitingResult = true;
        missingSince.reset();
        return obs == Observation::Hidden ? Action::DrawOnly : Action::ReloadAndDraw;
    }
};
}
