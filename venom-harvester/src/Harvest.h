#pragma once
#include <algorithm>
#include <cmath>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace harvest
{
    using ID = std::uint32_t;
    struct Key {
        ID actor{}, source{}, effect{}, instance{};
        auto operator<=>(const Key&) const = default;
    };
    struct Application {
        Key key{};
        ID bottle{};
        std::uint64_t order{};
    };
    struct Candidate {
        ID actor{}, bottle{};
        std::uint64_t order{};
        bool confirmed{};
    };

    // Pure rules are shared by the actual DLL and native regression tests.
    inline bool live(bool dispelled, bool inactive, bool failedCondition,
        float elapsed, float duration, bool executing)
    {
        if (dispelled || inactive || failedCondition || !std::isfinite(elapsed) ||
            !std::isfinite(duration) || duration < 0) return false;
        // A final damage tick can execute exactly at the duration boundary.
        // That effect is eligible only inside its callback, not afterward.
        return duration > 0 ? elapsed < duration || (executing && elapsed == duration) : executing;
    }

    inline void weaken(float& magnitude, float& duration, bool noMagnitude)
    {
        if (noMagnitude) {
            if (std::isfinite(duration) && duration > 0) duration *= 0.75f;
        } else if (std::isfinite(magnitude)) magnitude *= 0.75f;
    }

    class Ledger
    {
    public:
        std::uint64_t sequence{};
        std::map<Key, Application> effects;
        std::map<ID, Candidate> candidates;
        std::set<ID> rewarded;

        void clear() { sequence = 0; effects.clear(); candidates.clear(); rewarded.clear(); }
        const Application* find(const Key& key) const
        {
            const auto it = effects.find(key);
            return it == effects.end() ? nullptr : &it->second;
        }
        void begin(Key key, ID bottle, bool newStart)
        {
            if (!key.actor || !key.source || !key.effect || !bottle || rewarded.contains(key.actor)) return;
            // Update ticks and reload reconstruction preserve the original bottle
            // and application ordering (important when phial contents change).
            if (!newStart && effects.contains(key)) return;
            effects[key] = { key, bottle, ++sequence };
        }
        void offer(const Application& application, bool confirmed)
        {
            if (!application.bottle || rewarded.contains(application.key.actor)) return;
            auto& candidate = candidates[application.key.actor];
            if (application.order >= candidate.order)
                candidate = { application.key.actor, application.bottle, application.order,
                    confirmed || candidate.confirmed };
            else candidate.confirmed = candidate.confirmed || confirmed;
        }
        void end(const Key& key, bool dyingWhileActive)
        {
            if (const auto it = effects.find(key); it != effects.end()) {
                // Death can clear effects before TESDeathEvent is delivered.
                // Preserve only a dying actor's still-valid effect, not an
                // ordinary expiration or dispel. Killer still needs confirmation.
                if (dyingWhileActive) offer(it->second, false);
                effects.erase(it);
            }
        }
        bool death(ID actor, bool playerKiller, std::span<const Key> eligible)
        {
            if (!playerKiller || rewarded.contains(actor)) {
                candidates.erase(actor);
                return false;
            }
            for (const auto& key : eligible)
                if (key.actor == actor)
                    if (const auto app = find(key)) offer(*app, true);
            if (const auto it = candidates.find(actor); it != candidates.end()) {
                it->second.confirmed = true;
                return true;
            }
            return false;
        }
        std::optional<ID> claim(ID actor)
        {
            const auto it = candidates.find(actor);
            if (it == candidates.end() || !it->second.confirmed || !it->second.bottle ||
                rewarded.contains(actor)) return std::nullopt;
            const auto bottle = it->second.bottle;
            rewarded.insert(actor);   // Claim before inventory callbacks can re-enter.
            candidates.erase(it);
            std::erase_if(effects, [actor](const auto& pair) { return pair.first.actor == actor; });
            return bottle;
        }
        void forget(ID actor)
        {
            std::erase_if(effects, [actor](const auto& pair) { return pair.first.actor == actor; });
            candidates.erase(actor);
            rewarded.erase(actor);  // Actual reference deletion, not resurrection.
        }
    };

    // Explicit little-endian wire format: no C++ struct padding or pointers.
    inline std::vector<std::uint8_t> encode(const Ledger& ledger)
    {
        std::vector<std::uint8_t> out;
        const auto u32 = [&](std::uint32_t value) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
        };
        const auto u64 = [&](std::uint64_t value) { u32(static_cast<ID>(value)); u32(static_cast<ID>(value >> 32)); };
        u32(1); u64(ledger.sequence);
        u32(static_cast<ID>(ledger.effects.size()));
        u32(static_cast<ID>(ledger.candidates.size()));
        u32(static_cast<ID>(ledger.rewarded.size()));
        for (const auto& [key, app] : ledger.effects) {
            u32(key.actor); u32(key.source); u32(key.effect); u32(key.instance);
            u32(app.bottle); u64(app.order);
        }
        for (const auto& [actor, candidate] : ledger.candidates) {
            u32(actor); u32(candidate.bottle); u64(candidate.order); u32(candidate.confirmed ? 1 : 0);
        }
        for (auto actor : ledger.rewarded) u32(actor);
        return out;
    }

    template<class Resolve>
    inline std::optional<Ledger> decode(std::span<const std::uint8_t> bytes, Resolve resolve)
    {
        std::size_t offset = 0;
        bool good = true;
        const auto u32 = [&]() -> ID {
            if (offset + 4 > bytes.size()) { good = false; return 0; }
            ID value = 0;
            for (int i = 0; i < 4; ++i) value |= static_cast<ID>(bytes[offset++]) << (i * 8);
            return value;
        };
        const auto u64 = [&]() -> std::uint64_t { const auto low = u32(); return low | (std::uint64_t{u32()} << 32); };
        if (u32() != 1) return std::nullopt;
        Ledger ledger;
        ledger.sequence = u64();
        const auto effects = u32(), candidates = u32(), rewarded = u32();
        constexpr ID limit = 1000000;
        if (!good || effects > limit || candidates > limit || rewarded > limit ||
            24ULL + 28ULL * effects + 20ULL * candidates + 4ULL * rewarded != bytes.size()) return std::nullopt;
        for (ID i = 0; i < effects; ++i) {
            Key key{ resolve(u32()), resolve(u32()), resolve(u32()), u32() };
            const auto bottle = resolve(u32());
            const auto order = u64();
            if (key.actor && key.source && key.effect && bottle && order && order <= ledger.sequence)
                ledger.effects[key] = { key, bottle, order };
        }
        for (ID i = 0; i < candidates; ++i) {
            const auto actor = resolve(u32()), bottle = resolve(u32());
            const auto order = u64();
            const auto confirmed = u32();
            if (confirmed > 1) return std::nullopt;
            if (actor && bottle && order && order <= ledger.sequence)
                ledger.candidates[actor] = { actor, bottle, order, confirmed == 1 };
        }
        for (ID i = 0; i < rewarded; ++i) if (const auto actor = resolve(u32())) ledger.rewarded.insert(actor);
        if (!good || offset != bytes.size()) return std::nullopt;
        // A completed reward always dominates stale pending entries.
        for (auto actor : ledger.rewarded) {
            ledger.candidates.erase(actor);
            std::erase_if(ledger.effects, [actor](const auto& pair) { return pair.first.actor == actor; });
        }
        return ledger;
    }
}
