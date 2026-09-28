#include "RewardState.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>

void require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main()
{
    ovation::Amounts base{};
    require(ovation::addTriple(base, 24, 25), "health reward accepted");
    require(ovation::addTriple(base, 107, 10), "speech modifier accepted");
    require(base[0] == 75 && base[4] == 30, "exactly three times base");
    require(!ovation::addTriple(base, 999, 10), "unknown actor value rejected");
    require(!ovation::addTriple(base, 24, -1), "negative reward rejected");
    require(!ovation::addTriple(base, 24, std::numeric_limits<float>::infinity()), "infinity rejected");

    ovation::State state;
    state.award(base, 100);
    require(!state.enter(100), "inn to its own town does not expire");
    require(!state.enter(100), "shops and houses within that town do not expire");
    require(!state.enter(0) && state.active, "leaving town preserves the reward");
    require(!state.enter(0), "wilderness and settlements without inns preserve it");
    require(state.enter(100) && !state.active, "returning to same town expires it");
    state.award(base, 100);
    require(state.enter(200), "direct fast travel to a different town expires it");
    state.award(base, 0);
    require(state.enter(200), "a roadside inn reward expires at next town");
    require(!state.enter(300), "no reward means nothing to expire");

    state.award(base, 100);
    auto stronger = base;
    stronger[0] = 150;
    state.award(stronger, 100);
    require(state.amounts[0] == 150, "repeat performance replaces, never stacks");
    state.enter(0);
    const auto saved = state;
    ovation::State restored;
    std::memcpy(&restored, &saved, sizeof(restored));
    require(restored.valid() && restored.active && restored.amounts[0] == 150,
        "save round trip preserves reward and amounts");
    require(!restored.enter(0) && restored.enter(100), "save in wilderness preserves return detection");
    state.award(base, 100);
    restored = state;
    require(!restored.enter(100), "reload in the original town is not an arrival");
    restored.amounts[0] = std::numeric_limits<float>::quiet_NaN();
    require(!restored.valid(), "corrupt save rejected");
    std::cout << "Standing Ovation reward, transition, and save-state tests passed.\n";
}
