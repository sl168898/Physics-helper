#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace corpse
{
    using ID = std::uint32_t;
    enum class Type : std::uint32_t { fire, frost, shock, poison };
    using Damage = std::array<double, 4>;
    constexpr double fraction = 0.25;
    constexpr std::uint32_t radiusFeet = 25; // Ordinator Corpse Gas's outer area.
    constexpr float radius = radiusFeet * 128.f / 6.f; // 533 1/3 units = 7.62 metres.
    constexpr std::uint32_t recordID = 0x43455850; // CEXP, version 1
    constexpr std::size_t maxActors = 4096, maxSources = 128;

    inline double healthLost(double before, double after)
    {
        if (!std::isfinite(before) || !std::isfinite(after) || before <= 0) return 0;
        return std::max(0.0, before - std::max(0.0, after));
    }
    inline double resistanceMultiplier(double resistance)
    {
        if (!std::isfinite(resistance)) return 0;
        return std::max(0.0, 1.0 - resistance / 100.0);
    }
    struct Origin {
        bool player{}, explosion{};
        ID poison{};
        Type type{Type::poison};
    };

    // Nested health/effect hooks observe overlapping intervals. A child claims
    // its actual health loss; ancestors only claim the remaining loss. Killing
    // evidence is settled at the outermost frame after all damage is counted.
    struct Frame {
        ID actor{};
        double before{}, children{};
        Origin origin;
        Frame* parent{};
        std::optional<Origin> killed{};
        struct Result { double damage{}; std::optional<Origin> death; };
        Result finish(double after, bool lethal)
        {
            const auto raw = healthLost(before, after);
            const auto own = std::max(0.0, raw - children);
            if (lethal && own > 0 && !killed) killed = origin;
            auto ancestor = parent;
            while (ancestor && ancestor->actor != actor) ancestor = ancestor->parent;
            if (ancestor) {
                ancestor->children += raw;
                if (killed && !ancestor->killed) ancestor->killed = killed;
                return {own, std::nullopt};
            }
            return {own, killed};
        }
    };
    enum class State : std::uint32_t { alive, pending, spent };
    struct Target {
        double total{};
        std::map<ID, Damage> oils;
        State state{State::alive};
        Damage blast{};
    };
    class Ledger {
    public:
        std::map<ID, Target> targets;
        void clear() { targets.clear(); }
        void forget(ID id) { targets.erase(id); }
        void resurrected(ID id) {
            const auto it = targets.find(id);
            if (it != targets.end() && it->second.state == State::spent) targets.erase(it);
        }
        void record(ID victim, const Origin& source, double damage)
        {
            if (!victim || !source.player || source.explosion || !std::isfinite(damage) || damage <= 0) return;
            if (!targets.contains(victim) && targets.size() >= maxActors) return;
            auto& t = targets[victim];
            if (t.state != State::alive) return;
            if (!std::isfinite(t.total + damage)) return;
            t.total += damage;
            if (source.poison && (t.oils.contains(source.poison) || t.oils.size() < maxSources))
                t.oils[source.poison][static_cast<unsigned>(source.type)] += damage;
        }
        bool killed(ID victim, const Origin& source)
        {
            const auto it = targets.find(victim);
            if (it == targets.end() || it->second.state != State::alive) return false;
            auto& t = it->second;
            t.state = State::spent; // Every observed killing blow consumes this life.
            if (!source.player || source.explosion || !source.poison || t.total <= 0) return false;
            const auto oil = t.oils.find(source.poison);
            if (oil == t.oils.end()) return false;
            double sum{};
            for (auto part : oil->second) sum += part;
            if (!(sum > 0) || !std::isfinite(sum)) return false;
            for (std::size_t i = 0; i < 4; ++i) t.blast[i] = t.total * fraction * (oil->second[i] / sum);
            t.state = State::pending;
            return true;
        }
        std::optional<Damage> claim(ID victim)
        {
            const auto it = targets.find(victim);
            if (it == targets.end() || it->second.state != State::pending) return std::nullopt;
            it->second.state = State::spent; // Before any damage callback can re-enter.
            return it->second.blast;
        }
    };

    inline std::vector<std::uint8_t> encode(const Ledger& ledger)
    {
        std::vector<std::uint8_t> out;
        const auto u32 = [&](std::uint32_t x) { for (unsigned i = 0; i < 4; ++i) out.push_back(x >> (i * 8)); };
        const auto number = [&](double x) {
            const auto bits = std::bit_cast<std::uint64_t>(x);
            u32(static_cast<std::uint32_t>(bits)); u32(static_cast<std::uint32_t>(bits >> 32));
        };
        u32(1); u32(static_cast<std::uint32_t>(ledger.targets.size()));
        for (const auto& [id, t] : ledger.targets) {
            u32(id); u32(static_cast<std::uint32_t>(t.state)); number(t.total);
            for (auto d : t.blast) number(d);
            u32(static_cast<std::uint32_t>(t.oils.size()));
            for (const auto& [source, damage] : t.oils) { u32(source); for (auto d : damage) number(d); }
        }
        return out;
    }
    template<class Resolve>
    std::optional<Ledger> decode(std::span<const std::uint8_t> bytes, Resolve resolve)
    {
        std::size_t pos{}; bool valid = true;
        const auto u32 = [&]() -> std::uint32_t {
            if (bytes.size() - pos < 4) { valid = false; return 0; }
            std::uint32_t n{};
            for (unsigned i = 0; i < 4; ++i) n |= std::uint32_t(bytes[pos++]) << (i * 8);
            return n;
        };
        const auto number = [&]() {
            const auto lo = u32(), hi = u32();
            const auto d = std::bit_cast<double>(std::uint64_t(lo) | (std::uint64_t(hi) << 32));
            if (!std::isfinite(d) || d < 0) valid = false;
            return d;
        };
        if (u32() != 1) return std::nullopt;
        const auto count = u32();
        if (!valid || count > maxActors) return std::nullopt;
        Ledger result;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto id = resolve(u32()), state = u32();
            if (state > 2) return std::nullopt;
            Target t; t.state = static_cast<State>(state); t.total = number();
            double blastTotal{};
            for (auto& d : t.blast) { d = number(); blastTotal += d; }
            if (!valid || blastTotal > t.total * fraction + 0.001) return std::nullopt;
            const auto sources = u32();
            if (sources > maxSources) return std::nullopt;
            for (std::uint32_t j = 0; j < sources; ++j) {
                const auto source = resolve(u32()); Damage damage{}; double sum{};
                for (auto& d : damage) { d = number(); sum += d; }
                if (!valid || sum > t.total + 0.001) return std::nullopt;
                if (source && !t.oils.emplace(source, damage).second) return std::nullopt;
            }
            if (!valid) return std::nullopt;
            if (id && !result.targets.emplace(id, std::move(t)).second) return std::nullopt;
        }
        if (!valid || pos != bytes.size()) return std::nullopt;
        return result;
    }
}
