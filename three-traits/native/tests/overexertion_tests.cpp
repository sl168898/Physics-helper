#include "Overexertion.h"
#include <cassert>
#include <cmath>
#include <limits>

int main() {
    traits::Overexertion drawback;
    assert(drawback.extraDamage(100, 100) == 0);
    drawback.shout();
    assert(drawback.extraDamage(100, 100) == 20);
    assert(drawback.extraDamage(100, 60) == 12); // 40 magic remains unchanged
    assert(drawback.extraDamage(100, 0) == 0);   // no physical component
    assert(drawback.extraDamage(0, 100) == 0);   // fully blocked
    assert(drawback.extraDamage(30, 100) == 6);  // never amplify over total
    assert(drawback.extraDamage(-1, 10) == 0);
    assert(drawback.extraDamage(10, -1) == 0);
    auto nan = std::numeric_limits<float>::quiet_NaN();
    auto inf = std::numeric_limits<float>::infinity();
    assert(drawback.extraDamage(nan, 100) == 0);
    assert(drawback.extraDamage(100, inf) == 0);
    drawback.tick(2.5f, true);
    assert(drawback.remaining == 0.5f && drawback.extraDamage(100, 100) == 20);
    drawback.shout();
    assert(drawback.remaining == 3 && drawback.extraDamage(100, 100) == 20);
    drawback.tick(0, true); drawback.tick(-1, true); drawback.tick(nan, true); drawback.tick(inf, true);
    assert(drawback.remaining == 3);
    drawback.tick(2.75f, true);
    assert(drawback.extraDamage(100, 100) == 20);
    drawback.tick(0.25f, true);
    assert(drawback.remaining == 0 && drawback.extraDamage(100, 100) == 0);
    drawback.shout(); drawback.tick(10, true);
    assert(drawback.remaining == 0);
    drawback.shout(); drawback.tick(0, false); // trait loss/death clears even with zero dt
    assert(drawback.remaining == 0);
    drawback.shout(); drawback.clear(); // load/new game reset
    assert(drawback.extraDamage(100, 100) == 0);
}
