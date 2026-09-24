#pragma once
#include <cstdint>
#include <utility>

namespace traits {
// A one-shot reservation, consumed only after the engine's player update.
// Recovery and Echoing Steel are reserved by the original attack, not repeated
// at release. Only stable form IDs are held across the callback boundary.
struct SkaldDeferred {
    struct Request {
        std::uint32_t shout = 0, spell = 0;
        explicit operator bool() const { return shout && spell; }
    };
    Request pending;
    bool waiting() const { return bool(pending); }
    bool queue(std::uint32_t shout, std::uint32_t spell) {
        if (waiting() || !shout || !spell) return false;
        pending = {shout, spell}; return true;
    }
    void clear() { pending = {}; }
    Request take(std::uint32_t storedShout, bool eligible, bool paused) {
        if (!eligible || pending.shout != storedShout) { clear(); return {}; }
        if (paused) return {};
        // Clear before engine callbacks can re-enter the runtime.
        return std::exchange(pending, {});
    }
};
}
