#include "RunicOverdrive.h"
#include <cassert>
#include <limits>
using namespace traits;
int main() {
    // Qualifying power-attack hit, then all independent rejection boundaries.
    assert(runicEligible(true, true, true, true, true, false, true));
    assert(!runicEligible(false, true, true, true, true, false, true));
    assert(!runicEligible(true, false, true, true, true, false, true));
    assert(!runicEligible(true, true, false, true, true, false, true)); // Skald/poisons
    assert(!runicEligible(true, true, true, false, true, false, true)); // armor/staff
    assert(!runicEligible(true, true, true, true, false, false, true));
    assert(!runicEligible(true, true, true, true, true, true, true));
    assert(!runicEligible(true, true, true, true, true, false, false));
    RunicHit slot{};
    {
        RunicHitScope outer(slot, {true, true, 0x14, 10, 20});
        assert(slot.matches(0x14, 10, 20));
        assert(slot.matches(0x14, 10, 0)); // native context supplies missing source
        assert(!slot.matches(0x14, 11, 20));
        assert(!slot.matches(0x15, 10, 20));
        assert(!slot.matches(0x14, 10, 21)); // other hand/weapon
        {
            RunicHitScope normal(slot, {true, false, 0x14, 10, 20});
            assert(slot.present && !slot.matches(0x14, 10, 20));
        }
        assert(slot.matches(0x14, 10, 20));
    }
    assert(!slot.present); // no leaked buff after the hit
    assert(runicScaling(0, false, false, 40, 4) == RunicScaling::magnitude); // DoT never x4
    assert(runicScaling(4, false, false, 20, 1) == RunicScaling::magnitude); // absorb
    assert(runicScaling(5, false, false, 30, 1) == RunicScaling::magnitude); // frost secondary value
    assert(runicScaling(0, false, false, -20, 5) == RunicScaling::magnitude);
    assert(runicScaling(21, true, false, 0, 3) == RunicScaling::duration);
    assert(runicScaling(23, true, false, 0, 10) == RunicScaling::duration);
    assert(runicScaling(21, true, true, 0, 3) == RunicScaling::none);
    assert(runicScaling(0, false, false, 0, 1) == RunicScaling::none); // Runemaster carrier
    assert(runicScaling(1, false, false, 100, 10) == RunicScaling::none); // scripted proc
    assert(runicScaling(40, false, false, 100, 10) == RunicScaling::none); // hazard is not a number
    assert(runicScaling(0, true, false, 20, 5) == RunicScaling::none);
    assert(runicScaling(0, false, false, std::numeric_limits<float>::infinity(), 5) == RunicScaling::none);
    assert(runicScaling(0, false, false, std::numeric_limits<float>::quiet_NaN(), 5) == RunicScaling::none);
    assert(runicScaling(0, false, false, std::numeric_limits<float>::max(), 5) == RunicScaling::none);
}
