#pragma once
#include <RE/Skyrim.h>
#include <array>

namespace runes::crafting {
// No confirmation-dialog dependency: this observes the inventory operations
// and requires a matching ItemCrafted event before transferring an enchantment.
void install(const std::array<RE::BGSConstructibleObject*, 8>& recipes, bool (*enabled)());
void reset();
}
