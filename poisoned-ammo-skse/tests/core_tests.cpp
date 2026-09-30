#include "Core.h"
#include "CoatingCore.h"
#include <cassert>
#include <iostream>
#include <random>

pa::Recipe example()
{
    return {{"arrows.esl", 0xF31}, {{"poison.esp", 0x8A1}, "Damage Health", 0, 0, {}, {}}, "Arrow [Damage Health]"};
}
template<class F> void rejects(F f) { bool rejected = false; try { f(); } catch (const std::exception&) { rejected = true; } assert(rejected); }
int main()
{
    // All perk combinations: dose count and per-bolt strength remain independent.
    for (bool r1 : {false,true}) for (bool r2 : {false,true}) for (bool md : {false,true}) {
        const auto mult = coating::strength(r1,r2);
        assert(mult == (r2 ? 2.0f : r1 ? 1.5f : 1.0f));
        const auto count = coating::doseCount(5,true,md);
        assert(count == (md ? 10u : 5u));
        assert(coating::doseCount(5,false,md)==5u);
        assert((pa::plan(100,1,count,1)==pa::Batch{1,static_cast<int>(count)}));
        float magnitude=10, duration=8;
        coating::scale(magnitude,duration,false,mult);
        assert(magnitude==10*mult && duration==8); // no magnitude AND duration multiplication
        magnitude=0;duration=8;
        coating::scale(magnitude,duration,true,mult);
        assert(magnitude==0 && duration==8*mult);
    }
    assert(coating::doseCount(10000,true,true)==20000);
    assert((pa::plan(30000,1,20000,1,30000)==pa::Batch{1,20000}));
    assert((pa::plan(30000,1,20001,1,30000)==pa::Batch{}));
    assert((pa::plan(7,1,coating::doseCount(5,true,true),1)==pa::Batch{1,7}));
    float markerMagnitude=0,markerDuration=0;
    assert(!coating::scale(markerMagnitude,markerDuration,true,2));
    float overflow=std::numeric_limits<float>::max();
    assert(!coating::scale(overflow,markerDuration,false,2));

    // ESL-flagged ESPs, low-range AE ESL records, regular ESPs and load-order moves.
    assert(pa::localID(0xFE0238A1, true) == 0x8A1);
    assert(pa::localID(0xFEABC8A1, true) == 0x8A1);
    assert(pa::localID(0xFE023123, true) == 0x123);
    assert(pa::localID(0x42018A1, false) == 0x2018A1);
    assert(pa::lower("Requiem - Poisons.ESP") == "requiem - poisons.esp");
    auto a = example(); auto b = a; b.poison.source.file = "different.esl";
    pa::Recipes rs{a, b}; assert(pa::existing(rs, b) == 1);
    assert(pa::decode(pa::encode(rs)) == rs);
    assert(pa::fingerprint(rs) == pa::fingerprint(pa::decode(pa::encode(rs))));
    assert(pa::fingerprint({}) == 0);
    assert(pa::fingerprint({a}) != pa::fingerprint({b}));

    // A player-crafted poison survives loss of its original FF form and last bottle.
    auto crafted = a; crafted.poison.source = {}; crafted.poison.flags = 1u << 17;
    crafted.poison.effects = {{{"effects.esp", 0xD00}, 37.25f, 0, 10, 12.5f}, {{"effects.esl", 0xFFF}, 0.5f, 4, 300, 90.0f}};
    crafted.poison.keywords = {{"keywords.esp", 0x801}};
    rs.push_back(crafted); assert(pa::decode(pa::encode(rs)) == rs);
    auto stronger = crafted; stronger.poison.effects[0].magnitude += 1;
    assert(!pa::existing(rs, stronger));
    for (std::size_t n = rs.size(); n < pa::capacity; ++n) { auto r = a; r.name += std::to_string(n); rs.push_back(r); }
    assert(pa::decode(pa::encode(rs)) == rs);
    rs.push_back(a); rejects([&] { pa::encode(rs); }); rs.pop_back();

    // Every schema-valid recipe can be saved even at the extreme field bounds.
    auto largest = crafted;
    const pa::Key longKey{std::string(256, 'a') + ".esp", 0xFFFFFF};
    largest.ammo = longKey; largest.name = std::string(240, 'n');
    largest.poison.name = std::string(240, 'p');
    largest.poison.effects.assign(64, {longKey, 1, 0, 0, 0});
    largest.poison.keywords.assign(128, longKey);
    assert(pa::valid(largest));
    const auto biggest = pa::encode(pa::Recipes(pa::capacity, largest));
    assert(biggest.size() < pa::maxSaveBytes);
    assert(pa::decode(biggest).size() == pa::capacity);

    auto bytes = pa::encode(rs);
    for (std::size_t n : {0u, 1u, 8u, 15u, 100u}) rejects([&] { pa::decode(std::span(bytes).first(n)); });
    auto broken = bytes; broken[30] ^= 1; rejects([&] { pa::decode(broken); });
    broken = bytes; broken.push_back(0); rejects([&] { pa::decode(broken); });
    crafted.poison.effects[0].magnitude = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { pa::encode({crafted}); });
    a.ammo.file = "UPPER.ESP"; assert(!pa::valid(a));
    a = example(); a.ammo.local = 0xFE123456; assert(!pa::valid(a));
    a = example(); a.poison.source.file.clear(); assert(!pa::valid(a));

    assert((pa::plan(100, 5, 1, 5) == pa::Batch{5, 5}));
    assert((pa::plan(100, 5, 5, 5) == pa::Batch{5, 25}));
    assert((pa::plan(7, 5, 5, 5) == pa::Batch{2, 7}));
    assert((pa::plan(100, 1, 5, 5) == pa::Batch{1, 5}));
    assert((pa::plan(-1, 5, 5, 5) == pa::Batch{}));
    assert((pa::plan(100, 5, 0, 5) == pa::Batch{}));
    assert((pa::plan(INT32_MAX, INT32_MAX, 10000, UINT32_MAX) == pa::Batch{1, 5000}));
    assert((pa::plan(100, 5, 5, 0) == pa::Batch{}));
    std::mt19937 rng(20260929);
    for (int i = 0; i < 20000; ++i) {
        const int ammo = static_cast<int>(rng() % 10000), poisons = static_cast<int>(rng() % 1000);
        const auto dose = 1 + rng() % 1000, requested = rng() % 10000;
        const auto p = pa::plan(ammo, poisons, dose, requested);
        assert(p.arrows <= ammo && p.bottles <= poisons && static_cast<unsigned>(p.bottles) <= requested);
        assert(p.arrows >= 0 && p.bottles >= 0 && p.arrows <= 5000);
        assert(static_cast<std::uint64_t>(p.bottles) * dose >= static_cast<unsigned>(p.arrows));
        if (p.arrows) assert(p.bottles == (p.arrows + dose - 1) / dose);
    }
    // Bounds-check hostile but checksum-valid inputs, not only checksum failures.
    for (int i = 0; i < 3000; ++i) {
        auto fuzz = pa::encode({example()}); const auto pos = 4 + rng() % (fuzz.size() - 8);
        fuzz[pos] ^= static_cast<std::uint8_t>(1 + rng() % 255);
        auto sum = pa::checksum(std::span(fuzz).first(fuzz.size() - 4));
        for (unsigned j = 0; j < 4; ++j) fuzz[fuzz.size() - 4 + j] = static_cast<std::uint8_t>(sum >> (j * 8));
        try { auto decoded = pa::decode(fuzz); assert(pa::decode(pa::encode(decoded)) == decoded); } catch (const std::runtime_error&) {}
    }
    std::cout << "PASS: combined perk matrix, bolt-only doses, rank precedence, finite scaling; ESL identity, save isolation, crafted effects, full pool, corruption, 20000 batch cases and 3000 parser mutations\n";
}
