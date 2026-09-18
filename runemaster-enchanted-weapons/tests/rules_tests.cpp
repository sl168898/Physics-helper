#include "Rules.h"
#include <cassert>
#include <limits>
#include <set>

int main() {
    using namespace runes;
    // Normal, power, sneak, and blocked strikes retain their normal rune behavior.
    for (auto flags : {0, 1, 2, 3, 8, 9, 10, 11}) assert(eligibleHit(true, true, false, flags));
    // Both shield and weapon bashes, spells, enchantment notifications and unarmed
    // hits must not create extra procs. A staff is not a physical rune weapon.
    for (int flags = 0; flags < 16; ++flags)
        if (flags & 4) assert(!eligibleHit(true, true, false, flags));
    assert(!eligibleHit(true, false, false, 0));
    assert(!eligibleHit(true, true, true, 0));
    assert(!eligibleHit(false, true, false, 0));
    assert(active(false, false, 29.99f, 30.f));
    assert(!active(false, false, 30.f, 30.f));
    assert(!active(false, false, 31.f, 30.f));
    assert(!active(true, false, 1.f, 30.f));
    assert(!active(false, true, 1.f, 30.f));
    assert(!active(false, false, std::numeric_limits<float>::quiet_NaN(), 30.f));
    assert(newer(0.1f, 30, 20.f, 31));
    assert(!newer(20.f, 31, 0.1f, 30));
    assert(newer(0.f, 32, 0.f, 31));
    assert(!newer(0.f, 31, 0.f, 32));
    assert(!newer(0.f, 31, 0.f, 31));
    assert(newer(0.f, 0, 0.f, 65535));
    assert(!newer(0.f, 65535, 0.f, 0));
    // Baleful Glow is sustained (the original VMAD property is None), despite
    // its high damage. Only five of the eight runes are single-use.
    assert(!definitions[0].singleUse && !definitions[5].singleUse && !definitions[7].singleUse);
    int singleUse = 0;
    std::set<std::uint32_t> buffIDs, spellIDs;
    for (auto def : definitions) {
        singleUse += def.singleUse;
        assert(buffIDs.insert(def.self).second);
        assert(buffIDs.insert(def.transfer).second);
        assert(spellIDs.insert(def.bridge).second);
    }
    assert(singleUse == 5 && buffIDs.size() == 16 && spellIDs.size() == 8);
    std::set<std::uint32_t> weapons;
    for (auto def : permanentDefinitions) {
        assert(weapons.insert(def.weapon).second);
        assert(spellIDs.insert(def.bridge).second);
        assert(def.cooldown >= 0x850 && def.cooldown <= 0x855);
    }
    assert(weapons.size() == 8 && spellIDs.size() == 16);
    assert(permanentDefinitions[1].weapon == 0x8A2); // Bow is matched by hit source.
    assert(permanentDefinitions[4].cooldown == 0x855); // Actual Baleful cooldown.
}
