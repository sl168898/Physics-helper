#include "Skald.h"
#include "Rules.h"
#include <cassert>
#include <limits>
int main() {
    using traits::Skald;
    assert(Skald::cooldown(49.99f) == 10);
    assert(Skald::cooldown(50) == 6);
    assert(Skald::cooldown(99.99f) == 6);
    assert(Skald::cooldown(100) == 3);
    Skald s;
    assert(!s.start(100, true, true, false, true, true)); // no stored shout
    s.choose(42);
    assert(!s.start(100, false, true, false, true, true));
    assert(!s.start(100, true, false, false, true, true)); // locked/removed shout
    assert(!s.start(100, true, true, true, true, true)); // power bash
    assert(!s.start(100, true, true, false, false, true)); // light attack
    assert(!s.start(100, true, true, false, true, false)); // ranged attack
    assert(s.start(0, true, true, false, true, true));
    assert(!s.start(100, true, true, false, true, true)); // duplicate or fast attack
    s.tick(9); s.choose(43);
    assert(s.remaining == 1); // switching does not bypass recovery
    s.choose(0); s.choose(42); assert(s.remaining == 1);
    s.tick(-1); s.tick(std::numeric_limits<float>::infinity()); assert(s.remaining == 1);
    Skald restored; restored.restore(s.shout, s.remaining);
    assert(!restored.start(100, true, true, false, true, true));
    restored.tick(1); assert(restored.start(100, true, true, false, true, true));
    assert(restored.remaining == 3);
    restored.restore(42, std::numeric_limits<float>::quiet_NaN()); assert(restored.remaining == 10);

    // Current attack snapshots a previously armed bonus. Its Skald shout arms
    // the FOLLOWING attack, never a later contact from the current swing.
    traits::Combat c;
    c.beginSwing(false, true, true, false, false, true);
    c.shoutFromAttack();
    assert(c.damage(false, true, true) == 1);
    c.beginSwing(false, true, true, false, false, true); // duplicate animation / other hand
    assert(c.damage(false, true, true) == 1); // second target / second hand
    c.endSwing(); c.tick(1);
    c.beginSwing(false, true, true, true, false, true);
    assert(c.damage(false, true, true) == 2);
    c.shoutFromAttack(); // proc again when its independent recovery has elapsed
    c.beginSwing(false, true, true, true, false, true);
    assert(c.damage(false, true, true) == 2); // no stacking to 4x
    c.endSwing(); c.tick(5.01f);
    c.beginSwing(false, true, true, false, false, true);
    assert(c.damage(false, true, true) == 1); // expired echo

    // Combo animation transitions may reach NextAttack without an idle frame.
    traits::Combat combo;
    combo.beginSwing(false, true, true, false, false, true);
    combo.shoutFromAttack();
    assert(combo.damage(false, true, true) == 1);
    combo.finishPhase(); combo.tick(0.5f);
    combo.beginSwing(false, true, true, false, false, true);
    assert(combo.damage(false, true, true) == 1.5f);
    assert(combo.damage(false, true, true) == 1.5f); // second contact, not another charge
    combo.finishPhase(); combo.beginSwing(false, true, true, false, false, true);
    assert(combo.damage(false, true, true) == 1);

    // Waiting longer than Echo's window loses the token, even if Skald's
    // separate ten-second recovery has not ended.
    traits::Combat slow;
    slow.beginSwing(false, true, true, true, false, true); slow.shoutFromAttack();
    slow.endSwing(); slow.tick(5.01f);
    slow.beginSwing(false, true, true, true, false, true);
    assert(slow.damage(false, true, true) == 1);
}
