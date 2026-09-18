#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace runes {
struct Definition {
    const char* name;
    std::uint32_t self, transfer, power, enchantment, impact, bridge;
    bool singleUse;
};
inline constexpr std::array<Definition, 8> definitions{{
    {"Stunning Blast",   0x803, 0xA05, 0x856, 0x837, 0x802, 0x800, false},
    {"Almighty Bolt",    0x80A, 0xA00, 0x85A, 0x838, 0x809, 0x801, true},
    {"Runestorm",        0x813, 0xA03, 0x85D, 0x83A, 0x812, 0x802, true},
    {"Sundering Inferno",0x818, 0xA06, 0x860, 0x83C, 0x817, 0x803, true},
    {"Havoc",            0x81E, 0xA02, 0x846, 0x83D, 0x81D, 0x804, true},
    {"Spectral Slash",   0x822, 0xA04, 0x848, 0x83E, 0x821, 0x805, false},
    {"Umbral Veil",      0x827, 0xA07, 0x84B, 0x840, 0x825, 0x806, true},
    {"Baleful Glow",     0x82B, 0xA01, 0x84E, 0x842, 0x82A, 0x807, false},
}};

struct PermanentDefinition {
    const char* name;
    std::uint32_t weapon, enchantment, impact, cooldown, recipe, bridge;
};
// Stable IDs are relative to RunemasterMagic.esl, except bridge (our ESP).
// Keep the actual cooldown SPELLS: Baleful's script uses 855 (30 seconds),
// despite the original description saying 20 seconds.
inline constexpr std::array<PermanentDefinition, 8> permanentDefinitions{{
    {"Sundering Runic Battleaxe", 0x8A1, 0x844, 0x835, 0x851, 0x8AD, 0x810},
    {"Almighty Runic Bow",       0x8A2, 0x839, 0x82D, 0x852, 0x8AE, 0x811},
    {"Spectral Runic Dagger",    0x8A3, 0x83F, 0x833, 0x854, 0x8AF, 0x812},
    {"Umbral Runic Greatsword",  0x8A4, 0x836, 0x832, 0x853, 0x8B0, 0x813},
    {"Baleful Runic Mace",       0x8A5, 0x843, 0x82E, 0x855, 0x8B1, 0x814},
    {"Tempest Runic Sword",      0x8A6, 0x845, 0x831, 0x851, 0x8B2, 0x815},
    {"Stunning Runic War Axe",   0x8A7, 0x841, 0x834, 0x854, 0x8B3, 0x816},
    {"Kinetic Runic Warhammer",  0x8A8, 0x83B, 0x830, 0x851, 0x8B4, 0x817},
}};

// TESHitEvent flags: power=1, sneak=2, bash=4, blocked=8.
// Enchantment/spell hits, unarmed attacks and bashes are not weapon strikes.
inline bool eligibleHit(bool actorsValid, bool weapon, bool staff, std::uint8_t flags) {
    return actorsValid && weapon && !staff && !(flags & 4);
}
inline bool active(bool inactive, bool dispelled, float elapsed, float duration) {
    return !inactive && !dispelled && std::isfinite(elapsed) && std::isfinite(duration) &&
        (duration <= 0.f || elapsed < duration);
}
// Simultaneous effects use Skyrim's per-effect sequence number, including wraparound.
inline bool newer(float age, std::uint16_t sequence, float previousAge, std::uint16_t previousSequence) {
    if (age < previousAge) return true;
    if (age > previousAge) return false;
    const auto distance = static_cast<std::uint16_t>(sequence - previousSequence);
    return distance != 0 && distance < 0x8000;
}
}
