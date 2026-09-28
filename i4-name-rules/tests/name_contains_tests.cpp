#include "Data/Config/NameContains.h"
#include <iostream>
#include <string>

int main()
{
    struct Case { const char* name; const char* keyword; bool ignoreCase; bool expected; };
    const Case cases[] = {
        {"Weapon Oil of Jarrin Crown", "weapon oil", true, true},
        {"weapon oil", "weapon oil", true, true},
        {"WEAPON OIL OF JARRIN CROWN", "weapon oil", true, true},
        {"Greater wEaPoN oIl of Jarrin Crown III", "weapon oil", true, true},
        {"Jarrin Crown Weapon Oil", "weapon oil", true, true},
        {"Weapon Oils", "weapon oil", true, true},
        {"Weapon Oil (3)", "weapon oil", true, true},
        {"\xE6\xAF\x92 Weapon Oil \xE6\xB2\xB9", "weapon oil", true, true},
        {"Poison of Weakness to Fire", "weapon oil", true, false},
        {"Oil for Weapons", "weapon oil", true, false},
        {"WeaponOil", "weapon oil", true, false},
        {"Weapon-Oil", "weapon oil", true, false},
        {"Weapon  Oil", "weapon oil", true, false},
        {"", "weapon oil", true, false},
        {"Weapon", "weapon oil", true, false},
        {"Weapon Oil", "", true, false},
        {"", "", false, false},
        {"Weapon Oil", "weapon oil", false, false},
        {"Weapon Oil", "Weapon Oil", false, true},
        {"Greater weapon oil", "weapon oil", false, true},
        {"Greater weapon oil", "WEAPON OIL", true, true}
    };
    int count = 0;
    for (const auto& test : cases) {
        if (Data::NameContains(test.name, test.keyword, test.ignoreCase) != test.expected) {
            std::cerr << "Mismatch for name: " << test.name << '\n';
            return 1;
        }
        ++count;
    }
    std::string longName(10000, 'x');
    longName += " Weapon Oil of Jarrin Crown";
    if (!Data::NameContains(longName, "weapon oil", true)) return 1;
    std::cout << count + 1 << " name matching checks passed\n";
    return 0;
}
