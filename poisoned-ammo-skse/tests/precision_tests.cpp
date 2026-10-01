#include "PrecisionCore.h"
#include <cassert>

int main()
{
    using namespace precision;
    assert(chance(0) == 25 && chance(10) == 35 && chance(25) == 50);
    assert(chance(75) == 100 && chance(90) == 100 && chance(150) == 100);
    assert(criticalBonus(20, 0) == 20);
    assert(criticalBonus(20, 100) == 60);
    assert(criticalBonus(20, 500) == 220);
    assert(criticalBonus(20, 1000) == 420);
    assert(criticalBonus(20, -10) == 20 && criticalBonus(0, 1000) == 0);
    assert(criticalBonus(20, std::numeric_limits<float>::infinity()) == 20);
    assert(std::isfinite(criticalBonus(std::numeric_limits<float>::max(), 1000)));
    // Native modifiers run first: an existing +10 turns 20 into 30, then x3.
    assert(criticalBonus(20 + 10, 100) == 90);

    Frame shot{{7, 0x14, 0xABC, 0xF01, 0x123, 0x456, 500}, 0x77};
    assert(eligible(&shot, 7, 0x14, 0xABC, 0x77, true));
    assert(!eligible(&shot, 8, 0x14, 0xABC, 0x77, true));
    assert(!eligible(&shot, 7, 0x14, 0xDEF, 0x77, true));
    assert(!eligible(&shot, 7, 0x15, 0xABC, 0x77, true));
    assert(!eligible(&shot, 7, 0x14, 0xABC, 0x78, true));
    assert(!eligible(&shot, 7, 0x14, 0xABC, 0x77, false));
    assert(!eligible(nullptr, 7, 0x14, 0xABC, 0x77, true));
    {
        FrameGuard outer(&shot);
        assert(current == &shot);
        { FrameGuard plain(nullptr); assert(!current); }
        assert(current == &shot);
        { Frame other{shot.shot, 0}; FrameGuard nested(&other); assert(current == &other); }
        assert(current == &shot);
    }
    assert(!current);

    ShotStore store;
    store.remember(123, shot.shot);
    shot.shot.stamina = 0; // Impact-time Stamina changes cannot alter the copy.
    assert(store.find(123)->stamina == 500);
    assert(!store.find(124));
    store.erase(123); assert(!store.find(123));
    for (std::uint32_t i = 1; i <= ShotStore::capacity + 1; ++i) store.remember(i, shot.shot);
    assert(store.size() == ShotStore::capacity && !store.find(1) && store.find(2));
    store.clear(); assert(!store.find(2) && store.size() == 0);
}
