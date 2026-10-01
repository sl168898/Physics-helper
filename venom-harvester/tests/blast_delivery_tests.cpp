#include "BlastDelivery.h"
#include <cassert>
#include <cmath>
#include <limits>

using namespace corpse;
using Result = BlastDelivery::Result;
int main()
{
    // Replay both nonzero requests from the user's no-damage log. A cast
    // request is not evidence of acceptance or Health loss.
    for (double value : {34.85652732849121, 17.410035133361816}) {
        BlastDelivery cast{1, Type::poison, value, {2}};
        assert(cast.accepted == 0);
        const auto q = cast.claim(2, 0);
        assert(q.result == Result::ready && std::abs(q.magnitude - value) < 0.00001);
        assert(cast.accepted == 0);
        assert(cast.claim(2, 0).result == Result::duplicate);
    }
    BlastDelivery cast{1, Type::fire, 50, {1, 2, 3, 4, 5, 6, 7}};
    assert(cast.claim(0, 0).result == Result::excluded);
    assert(cast.claim(1, 0).result == Result::excluded); // corpse
    assert(cast.claim(20, 0).result == Result::excluded); // player/follower/neutral/out of range
    assert(cast.claim(2, 50).magnitude == 25);
    assert(cast.claim(3, -50).magnitude == 75);
    assert(cast.claim(4, 100).result == Result::resisted);
    assert(cast.claim(5, 200).result == Result::resisted);
    assert(cast.claim(6, std::numeric_limits<double>::quiet_NaN()).result == Result::invalid);
    // Each type gets its own native cast and matching resistance.
    BlastDelivery frost{1, Type::frost, 50, {2}};
    assert(frost.claim(2, 0).magnitude == 50);
    // Each corpse gets a new claim set: overlapping bursts are independent.
    BlastDelivery next{3, Type::fire, 30, {2}};
    assert(next.claim(2, 0).magnitude == 30);
    BlastDelivery huge{1, Type::fire, std::numeric_limits<double>::max(), {2}};
    assert(huge.claim(2, -100).result == Result::invalid);
    // Diagnostics report actual capped Health loss, never requested/overkill damage.
    assert(healthLost(200, 200) == 0);
    assert(healthLost(200, 175) == 25);
    assert(healthLost(200, -4800) == 200);
}
