#include "Bindings.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace esd;
    const Key first{0x1234, 0x14, 17}, second{0x1234, 0x14, 18};
    const Binding one{0xABC, 0x123456}, two{0xABC, 0x654321};
    Bindings state;
    state.entries[first] = one;
    state.entries[second] = two;
    assert(state.find(first, 0xABC) == one);
    assert(state.find(second, 0xABC) == two); // identical base/enchantment, different donor
    assert(!state.find(first, 0xDEF));       // stripped/replaced enchantment cannot show stale text
    assert(!state.find(first, 0));
    const Key inChest{0x1234, 0x9876, 1};
    state.move(first, inChest);
    assert(!state.find(first, 0xABC));
    assert(state.find(inChest, 0xABC) == one);
    state.move(inChest, inChest);
    assert(state.find(inChest, 0xABC) == one);
    const auto wire = encode(inChest, one);
    Key decodedKey;
    Binding decodedValue;
    assert(decode(wire, decodedKey, decodedValue));
    assert(decodedKey == inChest && decodedValue == one);
    Bindings reloaded;
    reloaded.entries[decodedKey] = decodedValue;
    assert(reloaded.find(inChest, 0xABC) == one);
    auto invalid = wire;
    invalid[2] = 65536;
    assert(!decode(invalid, decodedKey, decodedValue));
    for (unsigned i : {0, 1, 2, 3, 4}) {
        invalid = wire; invalid[i] = 0;
        assert(!decode(invalid, decodedKey, decodedValue));
    }
    state.move(inChest, {0x1234, 0, 0});
    assert(!state.find(inChest, 0xABC));
    assert(state.find(second, 0xABC) == two);
    state.entries.clear();
    assert(!state.find(second, 0xABC)); // revert/new character isolation
    Catalog catalog;
    catalog.add(1, true, 10, "Burns enemies");
    catalog.add(1, true, 11, "Burns enemies");
    assert(catalog.find(1, true) == 10);
    assert(!catalog.find(1, false));
    catalog.add(1, true, 12, "Different artifact text");
    assert(!catalog.find(1, true));
    catalog.add(2, false, 20, "");
    catalog.add(2, false, 21, "Custom robe text");
    assert(!catalog.find(2, false));
    catalog.add(3, true, 30, "Custom text");
    catalog.add(3, true, 31, "");
    assert(!catalog.find(3, true));
    catalog.clear();
    assert(!catalog.find(1, true));
    std::cout << "PASS: per-instance isolation, enchantment guards, identity migration, serialization, revert and conservative old-swap recovery\n";
}
