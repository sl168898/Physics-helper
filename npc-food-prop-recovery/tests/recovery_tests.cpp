#include "RecoveryPolicy.h"
#include <cassert>
#include <iostream>
using namespace foodprop;

int main() {
    // A load alone must not display a prop before the animation's draw event.
    Policy p;
    assert(p.observe(Observation::Missing, 2000, true, true) == Action::None);
    p.onDraw();
    assert(p.observe(Observation::Missing, 2000, true, true) == Action::None);
    assert(p.observe(Observation::Missing, 3499, true, true) == Action::None);
    assert(p.observe(Observation::Missing, 3500, true, true) == Action::ReloadAndDraw);
    // Asynchronous loads and a healthy result must not produce duplicates.
    assert(p.observe(Observation::Pending, 4000, true, true) == Action::None);
    assert(p.observe(Observation::Healthy, 4500, true, true) == Action::None);
    assert(!p.awaitingResult);
    assert(p.attempts == 1);
    // A second failure must honor cooldown and retry limit.
    assert(p.observe(Observation::Missing, 5000, true, true) == Action::None);
    assert(p.observe(Observation::Missing, 13000, true, true) == Action::None);
    assert(p.observe(Observation::Missing, 13500, true, true) == Action::ReloadAndDraw);
    assert(p.observe(Observation::Missing, 15000, true, true) == Action::None);
    assert(p.observe(Observation::Missing, 30000, true, true) == Action::None);

    Policy hidden;
    hidden.onDraw();
    hidden.observe(Observation::Hidden, 2000, true, true);
    assert(hidden.observe(Observation::Hidden, 3500, true, true) == Action::DrawOnly);
    // A legitimate hide/unload wins over any pending recovery.
    hidden.onHide();
    assert(hidden.observe(Observation::Missing, 20000, true, true) == Action::None);

    for (auto obs : {Observation::Pending, Observation::Suppressed, Observation::Ambiguous}) {
        Policy q; q.onDraw();
        q.observe(Observation::Missing, 2000, true, true);
        assert(q.observe(obs, 4000, true, true) == Action::None);
        assert(q.observe(Observation::Missing, 5000, true, true) == Action::None);
        assert(q.observe(Observation::Missing, 6499, true, true) == Action::None);
        assert(q.observe(Observation::Missing, 6500, true, true) == Action::ReloadAndDraw);
    }
    // Combat / actor unload / menu suspension cannot accumulate a repair timer.
    Policy q; q.onDraw();
    q.observe(Observation::Missing, 2000, true, true);
    assert(q.observe(Observation::Missing, 4000, false, true) == Action::None);
    assert(q.observe(Observation::Missing, 5000, true, true) == Action::None);
    assert(q.observe(Observation::Missing, 6500, true, true) == Action::ReloadAndDraw);
    // Diagnostic-only mode never changes the world.
    Policy dry; dry.onDraw();
    for (std::uint64_t t = 0; t < 120000; t += 500)
        assert(dry.observe(Observation::Missing, t, true, false) == Action::None);
    assert(dry.attempts == 0);
    // Old sessions and expired sequences fail closed.
    Policy old(5000); old.onDraw();
    assert(old.expired(4000));
    assert(old.observe(Observation::Missing, 4000, true, true) == Action::None);
    assert(old.observe(Observation::Missing, 126000, true, true) == Action::None);
    std::cout << "Recovery policy: all lifecycle, loading, cooldown and diagnostic checks passed\n";
}
