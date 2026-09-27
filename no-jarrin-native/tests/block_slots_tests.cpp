#include "BlockSlots.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <vector>

void require(bool condition, const char* name)
{
    if (!condition) { std::cerr << "FAIL: " << name << '\n'; std::exit(1); }
}

int main()
{
    constexpr unsigned root = 0x1BCBC, marker = 0xFE123D66;
    // Root may be a plugin entry, an FLM/save addition, or occur more than once.
    // Distinct crops expose accidental index shifting on either side of it.
    for (unsigned rootIndex = 0; rootIndex < 5; ++rootIndex) {
        std::array<unsigned, 5> seeds{10, 20, 30, 40, 50};
        const std::array<unsigned, 5> crops{101, 102, 103, 104, 105};
        seeds[rootIndex] = root;
        const auto before = seeds;
        const auto* allocation = seeds.data();
        const auto changed = no_jarrin::blockSlots(seeds, root, marker);
        require(changed == 1, "one root blocked");
        require(seeds.data() == allocation && seeds.size() == crops.size(), "storage and length retained");
        for (unsigned i = 0; i < seeds.size(); ++i) {
            require(seeds[i] == (i == rootIndex ? marker : before[i]), "unrelated seed/crop pairing retained");
        }
        require(no_jarrin::blockSlots(seeds, root, marker) == 0, "idempotent reload");
    }
    std::vector<unsigned> duplicate{root, 11, root, 12, root};
    require(no_jarrin::blockSlots(duplicate, root, marker) == 3, "all duplicate roots blocked");
    require(duplicate == std::vector<unsigned>({marker, 11, marker, 12, marker}), "duplicates do not shift other seeds");
    duplicate.push_back(root); // A later FLM/save restore can reintroduce a root.
    require(no_jarrin::blockSlots(duplicate, root, marker) == 1, "late addition blocked");
    require(std::find(duplicate.begin(), duplicate.end(), root) == duplicate.end(), "root no longer present");

    std::vector<unsigned> empty;
    require(no_jarrin::blockSlots(empty, root, marker) == 0 && empty.empty(), "empty list");
    std::vector<unsigned> absent{11, 22, marker, 33};
    const auto absentBefore = absent;
    require(no_jarrin::blockSlots(absent, root, marker) == 0 && absent == absentBefore, "absent root unchanged");

    int ingredient = 1, quest = 2, other = 3;
    std::array<int*, 4> pointers{nullptr, &ingredient, &other, &ingredient};
    require(no_jarrin::blockSlots(pointers, &ingredient, &quest) == 2, "pointer storage");
    require(pointers == std::array<int*, 4>{nullptr, &quest, &other, &quest}, "null and other forms preserved");

    // No assumption about matching crop count or scripted suffix placement.
    std::vector<unsigned> mismatchedSeeds{11, root, 22};
    const std::vector<unsigned> mismatchedCrops{101, 102, 103, 104};
    require(no_jarrin::blockSlots(mismatchedSeeds, root, marker) == 1, "mismatched list still blocks root");
    require(mismatchedSeeds.size() == 3 && mismatchedCrops.size() == 4, "no attempted mismatch repair");
    std::cout << "PASS: all native slot policy checks; game-engine integration requires Skyrim.\n";
}
