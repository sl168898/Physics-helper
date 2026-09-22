#include "IronLungs.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using traits::quoteIronLungs;
    auto q = quoteIronLungs(400, 400, 1);
    assert(q.allowed && q.cost == 100 && q.bonus == 100);
    q = quoteIronLungs(100, 400, 1);
    assert(q.allowed && q.cost == 100 && q.bonus == 25);
    assert(!quoteIronLungs(99.99f, 400, 1).allowed);
    assert(!quoteIronLungs(0, 400, 1).allowed);
    q = quoteIronLungs(300, 400, 0.8f);
    assert(q.allowed && q.cost == 80 && q.bonus == 75);
    assert(std::abs(quoteIronLungs(300, 400, 0.6f).cost - 60) < 0.0001f);
    assert(std::abs(quoteIronLungs(300, 400, 1.2f).cost - 120) < 0.0001f);
    q = quoteIronLungs(120, 400, 1.2f);
    assert(q.allowed && q.cost == 120);
    assert(!quoteIronLungs(std::nextafter(120.f, 0.f), 400, 1.2f).allowed);
    for (float reduction : {0.4f, 0.1f, 0.f, -1.f}) {
        q = quoteIronLungs(40, 400, reduction);
        assert(q.allowed && q.cost == 40 && q.bonus == 10);
        assert(!quoteIronLungs(39.99f, 400, reduction).allowed);
    }
    // Fortify Stamina changes the maximum, and hence the actual threshold.
    assert(!quoteIronLungs(100, 500, 1).allowed);
    assert(quoteIronLungs(125, 500, 1).allowed);
    // A later release checks current resources again, not the key-down quote.
    assert(quoteIronLungs(150, 400, 1).allowed);
    assert(!quoteIronLungs(90, 400, 1).allowed);
    // Damage comes from before payment, independent of the cost discount.
    float stamina = 400;
    for (float bonus : {100.f, 75.f, 50.f, 25.f}) {
        q = quoteIronLungs(stamina, 400, 1);
        assert(q.allowed && q.bonus == bonus);
        stamina -= q.cost;
    }
    assert(stamina == 0 && !quoteIronLungs(stamina, 400, 1).allowed);
    traits::IronLungsCast first, second;
    first.quote = quoteIronLungs(400, 400, 1);
    second.quote = quoteIronLungs(300, 400, 1);
    assert(first.claimTarget(42));
    assert(!first.claimTarget(42)); // multiple effects/projectiles cannot double-hit
    assert(first.claimTarget(43));  // the same shout can damage several targets
    assert(second.claimTarget(42)); // a later shout can damage the same enemy
    assert(first.quote.bonus == 100 && second.quote.bonus == 75);
    assert(!first.claimTarget(0));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    for (float invalid : {nan, inf, -inf}) {
        assert(!quoteIronLungs(invalid, 400, 1).allowed);
        assert(!quoteIronLungs(400, invalid, 1).allowed);
        assert(!quoteIronLungs(400, 400, invalid).allowed);
    }
    assert(!quoteIronLungs(100, 0, 1).allowed);
    assert(!quoteIronLungs(-1, 400, 1).allowed);
    assert(!quoteIronLungs(400, 400, 5).allowed);
}
