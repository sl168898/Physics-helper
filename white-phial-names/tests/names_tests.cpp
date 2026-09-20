#include "Names.h"
#include <cassert>
#include <iostream>
int main() {
    phial::Name n;
    assert(n.forPotion(0xFF001234).empty());
    n.assign(0xFF001234, "Kindling Oil", "Weakness to Fire");
    assert(n.forPotion(0xFF001234) == "Kindling Oil");
    assert(n.forPotion(0xFF005678).empty());
    const auto bottledEarlier = n.forPotion(0xFF001234);
    n.assign(0xFF001234, "Cinder Oil", "Weakness to Fire"); // Different named batch of the SAME form.
    assert(n.forPotion(0xFF001234) == "Cinder Oil" && bottledEarlier == "Kindling Oil");
    n.assign(0xFF001234, "Weakness to Fire", "Weakness to Fire");
    assert(n.forPotion(0xFF001234).empty()); // Ordinary batch must not inherit an old name.
    n.assign(0xFF001234, "寒霜之油", "Weakness to Frost");
    assert(n.restore(n.potion, n.text, [](auto) { return 0xFF009999; }));
    assert(n.forPotion(0xFF009999) == "寒霜之油" && n.forPotion(0xFF001234).empty());
    assert(!n.restore(0xFF009999, "Name", [](auto) { return 0; }));
    assert(n.text.empty() && n.potion == 0);
    assert(!n.restore(1, std::string(phial::maxNameBytes + 1, 'x'), [](auto id) { return id; }));
    assert(!n.restore(1, std::string("bad\0name", 8), [](auto id) { return id; }));
    n.assign(2, "Name", "Original"); n = {}; // Save revert / another character.
    assert(n.forPotion(2).empty());
    std::cout << "PASS: exact potion identity, separate batches, ordinary-name reset, UTF-8, load-ID resolution, invalid data and save isolation\n";
}
