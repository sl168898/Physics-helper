#include "Bank.h"
#include "NamedKeywords.h"
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
    // Frozen bytes produced by the released 2.0.1 encoder. Existing ESS
    // fingerprints must remain valid, not just round-trip through new code.
    const std::string legacyHex = "01000000010000000d000000456d62657262616e65204f696c22000000436c75747465722f506f74696f6e732f506f69736f6e426f74746c6530312e6e69660000000000000000000000000000003f000000007b00000000000200000000000000000000000000000000000000000000000000000000000a000000536b7972696d2e65736d443f01000000000000000000000000000000000000000000000000000000000000000000010000000a000000536b7972696d2e65736deccd0800020000000a000000536b7972696d2e65736d5a60040000001642cdcca041000000003d0000000b0000004578616d706c652e6573700a0800000000704100004041000000000f00000060b03bc3";
    Bytes legacy;
    for (std::size_t i = 0; i < legacyHex.size(); i += 2)
        legacy.push_back(static_cast<std::uint8_t>(std::stoul(legacyHex.substr(i, 2), nullptr, 16)));
    const Bank oldBank{{example()}};
    assert(decode(legacy) == oldBank);
    assert(encode(oldBank) == legacy);
    assert(fingerprint(oldBank) == 0xC33BB060);

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

    // The reported OCF tags must survive save/reload and a different runtime
    // FormID assignment. Resolution uses the current provider's keyword.
    struct Keyword { std::uint32_t id; } firstPotion{0xFF002135}, firstBottle{0xFF002159},
        nextPotion{0xFF005321}, nextBottle{0xFF005328};
    NamedKeywords<Keyword> first, next;
    first.add("OCF_VesselBottlePotion", &firstPotion);
    first.add("OCF_VesselBottle", &firstBottle);
    next.add("OCF_VesselBottlePotion", &nextPotion);
    next.add("OCF_VesselBottle", &nextBottle);
    auto named = a;
    named.namedKeywords = {"OCF_VesselBottlePotion", "OCF_VesselBottle"};
    Bank namedBank{{named}};
    const auto namedBytes = encode(namedBank);
    assert(namedBytes[0] == 2);
    auto restored = decode(namedBytes);
    assert(restored == namedBank && encode(restored) == namedBytes);
    assert(next.resolve(restored.liquids[0].namedKeywords[0]) == &nextPotion);
    assert(next.resolve(restored.liquids[0].namedKeywords[1]) == &nextBottle);
    assert(first.resolve(named.namedKeywords[0])->id != next.resolve(named.namedKeywords[0])->id);
    assert(namedBank.append(restored.liquids[0]) == 0);
    assert(bankFromCosave(cosave(namedBytes)) == namedBytes);
    for (std::size_t i = 0; i < namedBytes.size(); ++i) {
        rejects([&] { (void)decode(std::span(namedBytes).first(i)); });
        auto bad = namedBytes; bad[i] ^= 1; rejects([&] { (void)decode(bad); });
    }
    NamedKeywords<Keyword> missing, ambiguous;
    rejects([&] { (void)missing.resolve("OCF_VesselBottlePotion"); });
    rejects([&] { (void)missing.resolve(""); });
    ambiguous.add("OCF_VesselBottlePotion", &firstPotion);
    ambiguous.add("ocf_vesselbottlepotion", &nextPotion);
    rejects([&] { (void)ambiguous.resolve("OCF_VesselBottlePotion"); });
    first.add("OCF_VesselBottlePotion", &firstPotion); // Same object twice is not ambiguous.
    assert(first.resolve("ocf_vesselbottlepotion") == &firstPotion);
    invalid = a; invalid.namedKeywords = {""};
    rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid.namedKeywords = {std::string(513, 'x')};
    rejects([&] { (void)encode(Bank{{invalid}}); });
    invalid.namedKeywords.assign(256, "OCF_VesselBottle"); // Plus the original static keyword.
    rejects([&] { (void)encode(Bank{{invalid}}); });
    // Adding a runtime-keyword recipe upgrades the bank without editing any
    // previously protected definition or moving its slot.
    auto upgraded = oldBank;
    assert(upgraded.append(named) == 1);
    assert(decode(encode(upgraded)).liquids[0] == oldBank.liquids[0]);
    assert(encode(oldBank) == legacy);
    std::cout << "Protected bank: legacy bytes/fingerprint, runtime keyword reload/identity, ambiguity refusal, codec, corruption, immutability, capacity and save isolation passed\n";
}
