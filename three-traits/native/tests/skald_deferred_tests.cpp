#include "SkaldDeferred.h"
#include "Skald.h"
#include "Rules.h"
#include <algorithm>
#include <cassert>

int main() {
    traits::SkaldDeferred pending;
    traits::Skald recovery;
    traits::Combat combat;
    recovery.choose(42);
    float stamina = 120;
    unsigned casts = 0;
    // The actual queue is exercised through a pre-handler event, native cost,
    // and post-update release. No engine healing implementation is mocked as
    // proof of game behavior: this fixture verifies only the chosen ordering.
    combat.beginSwing(false, true, true, true, false, true);
    assert(recovery.start(100, true, true, false, true, true));
    assert(pending.queue(42, 99));
    combat.shoutFromAttack();
    assert(casts == 0 && stamina == 120);
    assert(!pending.queue(42, 99)); // duplicate animation/other hand
    assert(!recovery.start(100, true, true, false, true, true));
    combat.beginSwing(false, true, true, true, false, true);
    assert(combat.damage(false, true, true) == 1); // no current-swing echo
    stamina -= 70; // original swing processing
    auto request = pending.take(42, true, false); // post-original player update
    assert(request && request.shout == 42 && request.spell == 99);
    assert(!pending.waiting()); // consumed before reentrant engine callbacks
    ++casts; stamina = std::min(120.f, stamina + 30.25f);
    assert(stamina == 80.25f && casts == 1);
    assert(!pending.take(42, true, false));
    assert(recovery.remaining == 3); // release never restarts or charges recovery
    assert(combat.damage(false, true, true) == 1);
    combat.finishPhase();
    combat.beginSwing(false, true, true, true, false, true);
    assert(combat.damage(false, true, true) == 2); // following attack

    assert(pending.queue(42, 99));
    assert(!pending.take(42, true, true)); // menus hold the queued release
    assert(pending.waiting());
    assert(pending.take(42, true, false));
    assert(pending.queue(42, 99));
    assert(!pending.take(43, true, false)); // changed stored shout
    assert(!pending.waiting());
    assert(pending.queue(42, 99));
    assert(!pending.take(42, false, true)); // death/trait loss cancels even paused
    assert(!pending.waiting());
    assert(pending.queue(42, 99)); pending.clear(); // load/new game/choice change
    assert(!pending.take(42, true, false));
    assert(!pending.queue(0, 99) && !pending.queue(42, 0));
}
