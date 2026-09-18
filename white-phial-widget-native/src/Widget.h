#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace widget
{
    enum class State { absent, empty, potion, poison };
    constexpr State classify(bool empty, bool potion, bool poison)
    {
        return potion ? State::potion : poison ? State::poison : empty ? State::empty : State::absent;
    }
    constexpr const char* label(State state)
    {
        switch (state) {
        case State::empty: return "Empty";
        case State::potion: return "Filled - Potion";
        case State::poison: return "Filled - Poison";
        default: return "Not carried";
        }
    }
    struct Settings
    {
        bool enabled = true;
        bool showLabel = true;
        bool hideAbsent = true;
        bool hideInMenus = true;
        float x = 78.125f;
        float y = 77.778f;
        float scale = 100.0f;
        float opacity = 90.0f;
        bool operator==(const Settings&) const = default;
    };
    inline bool valid(const Settings& value)
    {
        auto in = [](float n, float lo, float hi) { return std::isfinite(n) && n >= lo && n <= hi; };
        return in(value.x, 0, 100) && in(value.y, 0, 100) &&
            in(value.scale, 50, 250) && in(value.opacity, 10, 100);
    }
    struct Snapshot
    {
        bool active = false;
        bool ready = false;
        bool blocked = true; // Loading/title screen or HUD disabled: always hidden.
        bool inMenu = false;
        State state = State::absent;
    };
    constexpr bool visible(const Settings& cfg, const Snapshot& view, bool preview)
    {
        if (!view.active || view.blocked) return false;
        if (preview) return true;
        return cfg.enabled && view.ready && (!cfg.hideInMenus || !view.inMenu) &&
            (!cfg.hideAbsent || view.state != State::absent);
    }
    // Access only under the adapter's mutex. Old queued tasks cannot publish a
    // previous save's inventory or clear a newer session's pending request.
    struct Session
    {
        std::uint64_t epoch = 1;
        bool pending = false;
        Snapshot view;
        void reset(bool active) { ++epoch; pending = false; view = {}; view.active = active; }
        std::uint64_t request()
        {
            if (!view.active || pending) return 0;
            pending = true;
            return epoch;
        }
        bool finish(std::uint64_t ticket, const Snapshot& result)
        {
            if (ticket != epoch || !pending) return false;
            pending = false;
            view = result;
            return true;
        }
    };
    struct Layout { float x, y, unit; };
    inline Layout layout(const Settings& cfg, float width, float height, float textWidth)
    {
        // Position is a percentage of the actual display. Size scales with height;
        // measured labels and icon remain on screen at 16:9, ultrawide and 4K.
        const auto unit = std::max(0.1f, height / 720.0f * cfg.scale / 100.0f);
        const auto span = cfg.showLabel ? 62.0f * unit + textWidth : 56.0f * unit;
        return { std::clamp(width * cfg.x / 100, 0.0f, std::max(0.0f, width - span - 4)),
            std::clamp(height * cfg.y / 100, 0.0f, std::max(0.0f, height - 56 * unit - 4)), unit };
    }
}
