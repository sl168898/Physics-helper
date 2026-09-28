#include "bin/Integrations/I4/I4ResolveKey.h"
#include <cstdlib>
#include <iostream>
#include <unordered_map>

using namespace I4Integration;
static int checks = 0;
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    std::unordered_map<ResolveKey, std::string, ResolveKeyHash> icons;
    icons[{0xFF001234u, 0, "Weapon Oil"}] = "weapon_oil:amber";
    icons[{0xFF001234u, 0, "Fire Oil"}] = "weapon_oil:red";
    icons[{0xFF001234u, 0, "Healing Potion"}] = "potion_health";
    require(icons.size() == 3, "One base form retains three distinct batch icons");
    require(icons.at({0xFF001234u, 0, "Weapon Oil"}) == "weapon_oil:amber", "Cold resolution keeps the first batch");
    require(icons.at({0xFF001234u, 0, "Fire Oil"}) == "weapon_oil:red", "Cached wheel lookup keeps the second batch color");
    require(icons.at({0xFF001234u, 0, "Healing Potion"}) == "potion_health", "A third name retains its distinct icon");
    require(!icons.contains({0xFF001234u, 0, "Frost Oil"}), "Renaming cannot return the previous name's cached icon");
    require(!icons.contains({0xFF001234u, 0, ""}), "Legacy form-only lookup cannot borrow a named icon");
    require(!icons.contains({0xFF001234u, 0, "weapon oil"}), "Cache identity preserves case-sensitive I4 matching");
    require(!icons.contains({0xFF001235u, 0, "Weapon Oil"}), "Distinct base forms stay separate");
    require(!icons.contains({0xFF001234u, 7, "Weapon Oil"}), "Equipment signature remains part of cache identity");
    std::string temporary = "精力补给";
    icons[{0xFF001234u, 0, temporary}] = "potion_stam";
    temporary.assign("changed");
    require(icons.at({0xFF001234u, 0, "精力补给"}) == "potion_stam", "Cache owns its UTF-8 name after the input changes");
    icons.clear();
    require(icons.empty(), "Normal reset discards all name-dependent results");
    std::cout << checks << " icon-name cache checks passed\n";
}
