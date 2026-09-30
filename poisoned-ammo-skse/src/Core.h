#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace pa
{
    inline constexpr std::size_t capacity = 512;
    inline constexpr std::uint32_t ammoStart = 0x800, poisonStart = 0xA00, markerID = 0xC00;
    inline constexpr std::size_t maxSaveBytes = 32 * 1024 * 1024;
    inline constexpr std::string_view plugin = "PoisonedAmmoNative.esp";

    inline std::string lower(std::string s)
    {
        for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return s;
    }
    struct Key
    {
        std::string file;
        std::uint32_t local{};
        bool operator==(const Key&) const = default;
    };
    inline bool valid(const Key& k)
    {
        return !k.file.empty() && k.file.size() <= 260 && k.local <= 0xFFFFFF &&
            k.file.find('\0') == std::string::npos && k.file == lower(k.file) &&
            (k.file.ends_with(".esm") || k.file.ends_with(".esp") || k.file.ends_with(".esl"));
    }
    // Local ID masks depend on the source file's light flag, never its extension.
    inline std::uint32_t localID(std::uint32_t runtimeID, bool light)
    {
        return runtimeID & (light ? 0xFFFu : 0xFFFFFFu);
    }
    struct Effect
    {
        Key base;
        float magnitude{};
        std::uint32_t area{}, duration{};
        float cost{};
        bool operator==(const Effect&) const = default;
    };
    struct Poison
    {
        // A static poison is used directly; custom poisons have an empty source.
        // This preserves static poison conditions and any scripts on their MGEFs.
        Key source;
        std::string name;
        std::int32_t value{};
        std::uint32_t flags{};
        std::vector<Effect> effects;
        std::vector<Key> keywords;
        bool operator==(const Poison&) const = default;
        bool custom() const { return source.file.empty(); }
    };
    struct Recipe
    {
        Key ammo;
        Poison poison;
        std::string name;
        bool operator==(const Recipe&) const = default;
    };
    using Recipes = std::vector<Recipe>;
    inline bool valid(const Recipe& r)
    {
        if (!valid(r.ammo) || r.name.empty() || r.name.size() > 240 || r.name.find('\0') != std::string::npos ||
            r.poison.name.empty() || r.poison.name.size() > 240 || r.poison.name.find('\0') != std::string::npos) return false;
        if (!r.poison.custom()) return valid(r.poison.source) && r.poison.effects.empty() && r.poison.keywords.empty();
        if (r.poison.source.local || r.poison.effects.empty() || r.poison.effects.size() > 64 ||
            r.poison.keywords.size() > 128 || !(r.poison.flags & (1u << 17))) return false;
        for (const auto& e : r.poison.effects)
            if (!valid(e.base) || !std::isfinite(e.magnitude) || !std::isfinite(e.cost)) return false;
        return std::all_of(r.poison.keywords.begin(), r.poison.keywords.end(), [](const Key& k) { return valid(k); });
    }
    inline std::optional<std::size_t> existing(const Recipes& recipes, const Recipe& r)
    {
        const auto it = std::find(recipes.begin(), recipes.end(), r);
        if (it == recipes.end()) return {};
        return static_cast<std::size_t>(it - recipes.begin());
    }
    struct Batch { std::int32_t bottles{}, arrows{}; bool operator==(const Batch&) const = default; };
    inline Batch plan(std::int32_t ammo, std::int32_t poisons, std::uint32_t doses,
        std::uint32_t requested, std::uint32_t limit = 5000)
    {
        if (ammo <= 0 || poisons <= 0 || !doses || doses > 20000 || !requested || !limit) return {};
        const auto available = std::min<std::uint64_t>(static_cast<std::uint32_t>(ammo), std::min(limit, 100000u));
        const auto bottles = std::min({(available + doses - 1) / doses,
            static_cast<std::uint64_t>(poisons), static_cast<std::uint64_t>(requested)});
        return {static_cast<std::int32_t>(bottles), static_cast<std::int32_t>(std::min(available, bottles * doses))};
    }
    inline std::uint32_t checksum(std::span<const std::uint8_t> bytes)
    {
        std::uint32_t result = 2166136261u;
        for (const auto b : bytes) result = (result ^ b) * 16777619u;
        return result;
    }
    struct Writer
    {
        std::vector<std::uint8_t> bytes;
        void u32(std::uint32_t x) { for (unsigned i = 0; i < 4; ++i) bytes.push_back(static_cast<std::uint8_t>(x >> (i * 8))); }
        void f32(float x) { u32(std::bit_cast<std::uint32_t>(x)); }
        void str(const std::string& s) { u32(static_cast<std::uint32_t>(s.size())); bytes.insert(bytes.end(), s.begin(), s.end()); }
        void key(const Key& k) { str(k.file); u32(k.local); }
    };
    inline std::vector<std::uint8_t> encode(const Recipes& recipes)
    {
        if (recipes.size() > capacity) throw std::runtime_error("too many recipes");
        Writer w;
        w.u32(0x3150414E); w.u32(1); w.u32(static_cast<std::uint32_t>(recipes.size()));
        for (const auto& r : recipes) {
            if (!valid(r)) throw std::runtime_error("invalid recipe");
            w.key(r.ammo); w.str(r.name); w.key(r.poison.source); w.str(r.poison.name);
            w.u32(std::bit_cast<std::uint32_t>(r.poison.value)); w.u32(r.poison.flags);
            w.u32(static_cast<std::uint32_t>(r.poison.effects.size()));
            for (const auto& e : r.poison.effects) { w.key(e.base); w.f32(e.magnitude); w.u32(e.area); w.u32(e.duration); w.f32(e.cost); }
            w.u32(static_cast<std::uint32_t>(r.poison.keywords.size()));
            for (const auto& k : r.poison.keywords) w.key(k);
        }
        w.u32(checksum(w.bytes));
        if (w.bytes.size() > maxSaveBytes) throw std::runtime_error("save too large");
        return w.bytes;
    }
    class Reader
    {
        std::span<const std::uint8_t> bytes;
        std::size_t pos{};
    public:
        explicit Reader(std::span<const std::uint8_t> data) : bytes(data) {}
        std::uint32_t u32()
        {
            if (bytes.size() - pos < 4) throw std::runtime_error("truncated save");
            std::uint32_t v{};
            for (unsigned i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(bytes[pos++]) << (8 * i);
            return v;
        }
        float f32() { return std::bit_cast<float>(u32()); }
        std::string str()
        {
            const auto n = u32();
            if (n > 260 || n > bytes.size() - pos) throw std::runtime_error("invalid string length");
            std::string result(reinterpret_cast<const char*>(bytes.data() + pos), n); pos += n; return result;
        }
        Key key() { auto file = str(); return {std::move(file), u32()}; }
        bool done() const { return pos == bytes.size(); }
    };
    inline Recipes decode(std::span<const std::uint8_t> bytes)
    {
        if (bytes.size() < 16 || bytes.size() > maxSaveBytes) throw std::runtime_error("invalid save length");
        Reader tail(bytes.last(4));
        if (tail.u32() != checksum(bytes.first(bytes.size() - 4))) throw std::runtime_error("save checksum mismatch");
        Reader r(bytes.first(bytes.size() - 4));
        if (r.u32() != 0x3150414E || r.u32() != 1) throw std::runtime_error("unsupported save format");
        const auto n = r.u32();
        if (n > capacity) throw std::runtime_error("invalid slot count");
        Recipes recipes; recipes.reserve(n);
        for (std::uint32_t i = 0; i < n; ++i) {
            Recipe v; v.ammo = r.key(); v.name = r.str(); v.poison.source = r.key(); v.poison.name = r.str();
            v.poison.value = std::bit_cast<std::int32_t>(r.u32()); v.poison.flags = r.u32();
            const auto effects = r.u32();
            if (effects > 64) throw std::runtime_error("too many effects");
            for (std::uint32_t j = 0; j < effects; ++j) { Effect e; e.base = r.key(); e.magnitude = r.f32(); e.area = r.u32(); e.duration = r.u32(); e.cost = r.f32(); v.poison.effects.push_back(std::move(e)); }
            const auto keys = r.u32();
            if (keys > 128) throw std::runtime_error("too many keywords");
            for (std::uint32_t j = 0; j < keys; ++j) v.poison.keywords.push_back(r.key());
            if (!valid(v)) throw std::runtime_error("invalid saved recipe");
            recipes.push_back(std::move(v));
        }
        if (!r.done()) throw std::runtime_error("trailing data");
        return recipes;
    }
    // Exact integer within float precision; stored in the ESS Global Variables table.
    // Detects an absent/wrong SKSE co-save before any slot may be reused.
    inline float fingerprint(const Recipes& recipes)
    {
        if (recipes.empty()) return 0.0f;
        const auto bytes = encode(recipes);
        return static_cast<float>(1 + checksum(std::span(bytes).first(bytes.size() - 4)) % 16777214u);
    }
}
