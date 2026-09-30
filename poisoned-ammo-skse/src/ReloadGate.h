#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>

namespace pa::animation
{
    enum class ReloadEvent : std::uint64_t { none, start, stop };
    enum class ReloadResult { waiting, ready, cancelled };

    inline ReloadEvent reloadEventType(std::string_view tag)
    {
        if (tag == "reload" || tag == "reloadStart" || tag == "ReloadFast") return ReloadEvent::start;
        if (tag == "reloadStop" || tag == "reloadComplete") return ReloadEvent::stop;
        return ReloadEvent::none;
    }

    // One atomic word preserves the order and kind of the latest graph event.
    inline std::uint64_t nextReloadSignal(std::uint64_t previous, ReloadEvent kind)
    {
        return ((previous >> 2) + 1) * 4 + static_cast<std::uint64_t>(kind);
    }

    // Engine-independent gate used directly by the native adapter and tests.
    // No timer may release an animation while a reload is known to be active.
    class ReloadGate
    {
        std::uint64_t signal;
        float activeTime = 0, quietTime = 0, settledTime = 0;
        bool eventReloading = false, graphWasReloading = false;
        bool sawReload = false, completedReload = false, terminal = false;
    public:
        explicit ReloadGate(std::uint64_t checkpoint) : signal(checkpoint) {}

        ReloadResult advance(float delta, bool valid, bool paused, bool equipmentReady,
            std::optional<bool> graphReloading, std::uint64_t latestSignal)
        {
            if (terminal) return ReloadResult::waiting;
            if (!valid) { terminal = true; return ReloadResult::cancelled; }
            if (latestSignal != signal) {
                signal = latestSignal;
                const auto kind = static_cast<ReloadEvent>(signal & 3);
                if (kind == ReloadEvent::start || kind == ReloadEvent::stop) {
                    sawReload = true;
                    eventReloading = kind == ReloadEvent::start;
                    completedReload = kind == ReloadEvent::stop;
                    settledTime = quietTime = 0;
                }
            }
            if (graphReloading && *graphReloading) {
                sawReload = graphWasReloading = true;
                completedReload = false;
            } else if (graphReloading && graphWasReloading) {
                // Some animation replacers omit reloadStop. A witnessed true ->
                // false graph transition is also completion evidence.
                graphWasReloading = eventReloading = false;
                completedReload = true;
            }
            if (paused) { settledTime = quietTime = 0; return ReloadResult::waiting; }
            const auto dt = std::isfinite(delta) ? std::clamp(delta, 0.0f, 0.1f) : 0.0f;
            activeTime += dt;
            if (activeTime >= 15.0f) { terminal = true; return ReloadResult::cancelled; }
            if (!equipmentReady || eventReloading || graphReloading.value_or(false)) {
                settledTime = quietTime = 0;
                return ReloadResult::waiting;
            }
            quietTime += dt;
            // If equip causes no reload (e.g. AutoEquip=0), require a readable,
            // idle graph for a full second after the menu closes. Unknown state
            // never falls through to a fixed-delay animation.
            if (completedReload || (!sawReload && graphReloading.has_value() && quietTime >= 1.0f)) {
                settledTime += dt;
                if (settledTime >= 0.2f) { terminal = true; return ReloadResult::ready; }
            } else settledTime = 0;
            return ReloadResult::waiting;
        }
    };
}
