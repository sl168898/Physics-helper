#pragma once
#include <cstdint>

namespace runes::visuals {
// Visual identity follows the active rune, never the ordinary enchantment.
// A castable rune takes visual precedence over an intrinsic permanent rune.
inline std::uint32_t choose(std::uint32_t temporary, std::uint32_t permanent) {
    return temporary ? temporary : permanent;
}
inline bool eligible(bool loaded, bool alive, bool drawn, bool weapon, bool staff, bool unarmed) {
    return loaded && alive && drawn && weapon && !staff && !unarmed;
}
// Each actual weapon model gets one visual set. Reloaded 3D and camera changes
// replace it; repeated ticks against an unchanged model do not restart it.
inline bool reuse(std::uintptr_t oldNode, std::uintptr_t node,
    std::uint32_t oldWeapon, std::uint32_t weapon, bool sameAppearance, bool effectsLive) {
    return node && node == oldNode && weapon == oldWeapon && sameAppearance && effectsLive;
}
}
