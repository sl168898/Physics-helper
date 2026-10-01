#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>

namespace precision
{
    // All amounts are native entry-point outputs, not the shot's total damage.
    inline float chance(float ordinary) noexcept
    {
        return std::isfinite(ordinary) ? std::clamp(ordinary + 25.0f, 0.0f, 100.0f) : ordinary;
    }
    inline float criticalBonus(float ordinary, float stamina) noexcept
    {
        if (!std::isfinite(ordinary) || !std::isfinite(stamina) || ordinary <= 0) return ordinary;
        const double value = double(ordinary) * (1.0 + 0.02 * std::max(0.0, double(stamina)));
        return static_cast<float>(std::min(value, double(std::numeric_limits<float>::max())));
    }
    struct Shot
    {
        std::uint64_t epoch{};
        std::uint32_t shooter{}, weapon{}, ammo{}, baseAmmo{}, poison{};
        float stamina{};
    };
    struct Frame
    {
        Shot shot;
        std::uint32_t target{}; // Zero until the collision target is known.
    };
    inline thread_local const Frame* current{};
    class FrameGuard
    {
        const Frame* previous = current;
    public:
        explicit FrameGuard(const Frame* frame) noexcept { current = frame; }
        ~FrameGuard() { current = previous; }
        FrameGuard(const FrameGuard&) = delete;
        FrameGuard& operator=(const FrameGuard&) = delete;
    };
    inline bool eligible(const Frame* frame, std::uint64_t epoch, std::uint32_t owner,
        std::uint32_t weapon, std::uint32_t target, bool ownsPerk) noexcept
    {
        return frame && ownsPerk && frame->shot.epoch == epoch && owner != 0 && weapon != 0 &&
            frame->shot.shooter == owner && frame->shot.weapon == weapon &&
            (!frame->target || frame->target == target);
    }

    // Keys are engine reference handles, including their generation bits. Never
    // retain a strong projectile pointer. OnKill and save/load clear these entries.
    class ShotStore
    {
        struct Entry { Shot shot; std::uint64_t order; };
        std::unordered_map<std::uint32_t, Entry> entries;
        std::uint64_t next{};
    public:
        static constexpr std::size_t capacity = 4096;
        void remember(std::uint32_t handle, const Shot& shot)
        {
            if (!handle) return;
            if (!entries.contains(handle) && entries.size() >= capacity) {
                auto oldest = std::min_element(entries.begin(), entries.end(),
                    [](const auto& a, const auto& b) { return a.second.order < b.second.order; });
                entries.erase(oldest);
            }
            entries.insert_or_assign(handle, Entry{shot, ++next});
        }
        std::optional<Shot> find(std::uint32_t handle) const
        {
            const auto it = entries.find(handle);
            return it == entries.end() ? std::nullopt : std::optional(it->second.shot);
        }
        void erase(std::uint32_t handle) { entries.erase(handle); }
        void clear() { entries.clear(); next = 0; }
        std::size_t size() const { return entries.size(); }
    };
}
