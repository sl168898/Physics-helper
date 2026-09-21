#include "Bank.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace phial::storage;
Liquid example() {
    Liquid l; l.name = "Emberbane Oil"; l.model = "Clutter/Potions/PoisonBottle01.nif";
    l.weight = 0.5f; l.value = 123; l.flags = 1u << 17;
    l.effects = {{{"Skyrim.esm", 0x4605A}, 37.5f, 20.1f, 0, 61}, {{"Example.esp", 0x80A}, 15, 12, 0, 15}};
    l.keywords = {{"Skyrim.esm", 0x8CDEC}}; l.equip = {"Skyrim.esm", 0x13F44};
    return l;
}
template<class F> void rejects(F&& f) { bool caught = false; try { f(); } catch (const Error&) { caught = true; } assert(caught); }
Bytes cosave(const Bytes& bank) {
    Writer chunk; chunk.u32(bankRecord); chunk.u32(1); chunk.u32(static_cast<std::uint32_t>(bank.size()));
    chunk.data.insert(chunk.data.end(), bank.begin(), bank.end());
    Writer w; w.u32(0x45534B53); w.u32(1); w.u32(0); w.u32(0); w.u32(1);
    w.u32(pluginID); w.u32(1); w.u32(static_cast<std::uint32_t>(chunk.data.size()));
    w.data.insert(w.data.end(), chunk.data.begin(), chunk.data.end()); return w.data;
}
int main() {
    Bank b; auto a = example(); assert(b.append(a) == 0); assert(b.append(a) == 0);
    auto stronger = a; stronger.effects[0].magnitude = 99; assert(b.append(stronger) == 1);
    auto renamed = a; renamed.name = "Winter's Spite"; assert(b.append(renamed) == 2);
    assert(b.liquids[0] == a); // New assignments/strengths/names never change older bottles.
    const auto bytes = encode(b); assert(decode(bytes) == b);
    // All truncated inputs and single-byte corruptions must be rejected.
    for (std::size_t i = 0; i < bytes.size(); ++i) rejects([&] { (void)decode(std::span(bytes).first(i)); });
    for (std::size_t i = 0; i < bytes.size(); ++i) { auto bad = bytes; bad[i] ^= 1; rejects([&] { (void)decode(bad); }); }
    auto trailing = bytes; trailing.push_back(0); rejects([&] { (void)decode(trailing); });
    auto invalid = a; invalid.effects[0].magnitude = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid = a; invalid.effects.clear(); rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid = a; invalid.effects[0].base.file.clear(); rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid = a; invalid.name = std::string(4097, 'x'); rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid = a; invalid.effects[0].base.file = "../Example.esp"; rejects([&] { (void)encode(Bank{{invalid}}); });
    auto packed = cosave(bytes); assert(bankFromCosave(packed) == bytes);
    for (std::size_t i = 0; i < packed.size(); ++i) rejects([&] { (void)bankFromCosave(std::span(packed).first(i)); });
    auto duplicate = packed; // Two identical records in same plugin block must not be accepted.
    const auto chunk = Bytes(packed.begin() + 32, packed.end());
    duplicate[24] = 2; const auto length = static_cast<std::uint32_t>(chunk.size() * 2);
    for (int i = 0; i < 4; ++i) duplicate[28 + i] = static_cast<std::uint8_t>(length >> (i * 8));
    duplicate.insert(duplicate.end(), chunk.begin(), chunk.end());
    rejects([&] { (void)bankFromCosave(duplicate); });
    // Capacity refusal leaves the complete existing bank unchanged.
    Bank full;
    for (std::uint32_t i = 0; i < slotCount; ++i) { auto l = a; l.value = static_cast<int>(i); assert(full.append(l) == i); }
    const auto before = encode(full); auto extra = a; extra.value = -1;
    rejects([&] { full.append(extra); }); assert(encode(full) == before);
    assert(decode(encode(full)) == full);
    // Loading a different character/save starts from that bank only.
    Bank other; other.append(renamed); Bank loaded = decode(encode(other));
    assert(loaded.liquids.size() == 1 && loaded.liquids[0] == renamed);
    assert(fingerprint(b) != fingerprint(other)); assert(fingerprint(Bank{}) == 0);
    std::cout << "Protected liquid codec, immutability, bounds, corruption, co-save framing, capacity and isolation checks passed\n";
}
