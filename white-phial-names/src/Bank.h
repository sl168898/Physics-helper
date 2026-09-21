#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace phial::storage {
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint32_t firstSlot = 0x900, slotCount = 1792;
constexpr std::uint32_t pluginID = 0x57504E31, bankRecord = 0x57425331;
constexpr std::size_t maxBankBytes = 16 * 1024 * 1024;
struct Error : std::runtime_error { using runtime_error::runtime_error; };
struct Ref {
    std::string file;
    std::uint32_t local{};
    bool operator==(const Ref&) const = default;
};
struct Effect {
    Ref base;
    float magnitude{}, cost{};
    std::uint32_t area{}, duration{};
    bool operator==(const Effect&) const = default;
};
struct Liquid {
    std::string name, model, icon, messageIcon, secondaryIcon;
    float weight{}, addictionChance{};
    std::int32_t value{};
    std::uint32_t flags{};
    bool medicineRecord{};
    std::array<std::int16_t, 6> bounds{};
    Ref equip, addiction, consumeSound, pickupSound, putdownSound;
    std::vector<Ref> keywords;
    std::vector<Effect> effects;
    bool operator==(const Liquid&) const = default;
};
struct Bank {
    std::vector<Liquid> liquids;
    bool operator==(const Bank&) const = default;
    // Never evict/reuse a slot: bottles can be in an unloaded container.
    std::optional<std::size_t> find(const Liquid& liquid) const {
        auto it = std::find(liquids.begin(), liquids.end(), liquid);
        if (it == liquids.end()) return {};
        return static_cast<std::size_t>(it - liquids.begin());
    }
    std::size_t append(const Liquid& liquid) {
        if (auto i = find(liquid)) return *i;
        if (liquids.size() >= slotCount) throw Error("All 1792 protected liquid slots are occupied");
        liquids.push_back(liquid);
        return liquids.size() - 1;
    }
};
inline std::uint32_t crc32(std::span<const std::uint8_t> bytes) {
    std::uint32_t crc = ~0u;
    for (auto b : bytes) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
struct Writer {
    Bytes data;
    void u32(std::uint32_t x) { for (int i = 0; i < 4; ++i) data.push_back(static_cast<std::uint8_t>(x >> (i * 8))); }
    void f32(float x) { if (!std::isfinite(x)) throw Error("Non-finite liquid statistic"); u32(std::bit_cast<std::uint32_t>(x)); }
    void str(const std::string& x) {
        if (x.size() > 4096 || x.find('\0') != x.npos) throw Error("Invalid liquid text");
        u32(static_cast<std::uint32_t>(x.size())); data.insert(data.end(), x.begin(), x.end());
    }
    void ref(const Ref& r) {
        if ((r.file.empty() != (r.local == 0)) || r.local > 0xFFFFFF || r.file.size() > 260 ||
            r.file.find_first_of("/\\:") != r.file.npos) throw Error("Invalid persistent form reference");
        str(r.file); u32(r.local);
    }
    void liquid(const Liquid& l) {
        if (l.effects.empty() || l.effects.size() > 128 || l.keywords.size() > 256 || l.weight < 0)
            throw Error("Unsupported liquid definition");
        str(l.name); str(l.model); str(l.icon); str(l.messageIcon); str(l.secondaryIcon);
        f32(l.weight); f32(l.addictionChance); u32(std::bit_cast<std::uint32_t>(l.value)); u32(l.flags); u32(l.medicineRecord ? 1 : 0);
        for (auto b : l.bounds) u32(std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(b)));
        ref(l.equip); ref(l.addiction); ref(l.consumeSound); ref(l.pickupSound); ref(l.putdownSound);
        u32(static_cast<std::uint32_t>(l.keywords.size())); for (auto& k : l.keywords) ref(k);
        u32(static_cast<std::uint32_t>(l.effects.size()));
        for (auto& e : l.effects) {
            if (e.base.file.empty()) throw Error("Missing magic effect");
            ref(e.base); f32(e.magnitude); f32(e.cost); u32(e.area); u32(e.duration);
        }
    }
};
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t at{};
    std::span<const std::uint8_t> take(std::size_t n) {
        if (n > data.size() - at) throw Error("Truncated data");
        auto out = data.subspan(at, n); at += n; return out;
    }
    std::uint32_t u32() {
        auto b = take(4); return b[0] | (std::uint32_t(b[1]) << 8) | (std::uint32_t(b[2]) << 16) | (std::uint32_t(b[3]) << 24);
    }
    float f32() { auto f = std::bit_cast<float>(u32()); if (!std::isfinite(f)) throw Error("Non-finite statistic"); return f; }
    std::string str() {
        auto n = u32(); if (n > 4096) throw Error("Oversized text");
        auto b = take(n); std::string s(b.begin(), b.end());
        if (s.find('\0') != s.npos) throw Error("Invalid text");
        return s;
    }
    Ref ref() { Ref r{str(), u32()}; Writer check; check.ref(r); return r; }
    Liquid liquid() {
        Liquid l;
        l.name = str(); l.model = str(); l.icon = str(); l.messageIcon = str(); l.secondaryIcon = str();
        l.weight = f32(); l.addictionChance = f32(); l.value = std::bit_cast<std::int32_t>(u32()); l.flags = u32();
        const auto med = u32(); if (med > 1) throw Error("Invalid record flag"); l.medicineRecord = med != 0;
        for (auto& b : l.bounds) {
            const auto value = std::bit_cast<std::int32_t>(u32());
            if (value < -32768 || value > 32767) throw Error("Invalid bounds");
            b = static_cast<std::int16_t>(value);
        }
        l.equip = ref(); l.addiction = ref(); l.consumeSound = ref(); l.pickupSound = ref(); l.putdownSound = ref();
        auto n = u32(); if (n > 256) throw Error("Too many keywords");
        for (std::uint32_t i = 0; i < n; ++i) l.keywords.push_back(ref());
        n = u32(); if (n == 0 || n > 128) throw Error("Invalid effect count");
        for (std::uint32_t i = 0; i < n; ++i) l.effects.push_back({ref(), f32(), f32(), u32(), u32()});
        Writer check; check.liquid(l); return l;
    }
};
inline Bytes encode(const Bank& bank) {
    if (bank.liquids.size() > slotCount) throw Error("Oversized bank");
    Writer w; w.u32(1); w.u32(static_cast<std::uint32_t>(bank.liquids.size()));
    for (auto& l : bank.liquids) w.liquid(l);
    w.u32(crc32(w.data));
    if (w.data.size() > maxBankBytes) throw Error("Liquid bank exceeds size limit");
    return w.data;
}
inline Bank decode(std::span<const std::uint8_t> bytes) {
    if (bytes.size() < 12 || bytes.size() > maxBankBytes) throw Error("Invalid bank length");
    Reader r{bytes}; if (r.u32() != 1) throw Error("Unsupported bank version");
    auto n = r.u32(); if (n > slotCount) throw Error("Oversized bank");
    Bank b; for (std::uint32_t i = 0; i < n; ++i) b.liquids.push_back(r.liquid());
    auto crc = r.u32();
    if (r.at != bytes.size() || crc32(bytes.first(bytes.size() - 4)) != crc) throw Error("Bank checksum or length mismatch");
    return b;
}
inline std::uint32_t fingerprint(const Bank& b) {
    if (b.liquids.empty()) return 0;
    const auto data = encode(b); Reader r{std::span(data).last(4)}; return r.u32();
}
// Read only our record. The SKSE framing is documented in ianpatt/skse64
// Serialization.cpp v2.2.6. Reject duplicates and truncation; never merge saves.
inline std::optional<Bytes> bankFromCosave(std::span<const std::uint8_t> data) {
    Reader r{data}; if (r.u32() != 0x45534B53 || r.u32() != 1) throw Error("Invalid SKSE co-save header");
    r.u32(); r.u32(); const auto plugins = r.u32(); if (plugins > 65536) throw Error("Invalid plugin count");
    std::optional<Bytes> found;
    for (std::uint32_t p = 0; p < plugins; ++p) {
        auto id = r.u32(); auto count = r.u32(); auto length = r.u32(); Reader block{r.take(length)};
        if (id != pluginID) continue;
        if (count > 65536) throw Error("Invalid chunk count");
        for (std::uint32_t c = 0; c < count; ++c) {
            auto type = block.u32(); auto version = block.u32(); auto size = block.u32(); auto bytes = block.take(size);
            if (type == bankRecord) {
                if (found || version != 1 || size > maxBankBytes) throw Error("Invalid or duplicate liquid bank");
                found = Bytes(bytes.begin(), bytes.end());
            }
        }
        if (block.at != block.data.size()) throw Error("Invalid plugin block length");
    }
    if (r.at != r.data.size()) throw Error("Trailing co-save data");
    return found;
}
}
