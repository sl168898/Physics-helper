#include "ReloadGate.h"
#include <cassert>
#include <iostream>
#include <limits>

using namespace pa::animation;
constexpr auto wait = ReloadResult::waiting;
constexpr auto ready = ReloadResult::ready;
constexpr auto cancel = ReloadResult::cancelled;

struct Simulation
{
    std::uint64_t signal = 0;
    ReloadGate gate{signal};
    bool valid = true, paused = false, equipped = true;
    std::optional<bool> reloading = false;
    int deliveries = 0, cancellations = 0;
    explicit Simulation(bool expected = true) : gate(signal, expected) {}
    void event(std::string_view tag) {
        const auto kind = reloadEventType(tag);
        if (kind != ReloadEvent::none) signal = nextReloadSignal(signal, kind);
    }
    ReloadResult tick(float dt = 0.05f) {
        const auto result = gate.advance(dt, valid, paused, equipped, reloading, signal);
        deliveries += result == ready;
        cancellations += result == cancel;
        return result;
    }
    void frames(int count) { while (count--) tick(); }
};

int main()
{
    // Menu time must not count as reload/settle time, even for a long-open menu.
    for (const auto speed : { 4, 20, 160 }) {
        Simulation s; s.paused = true; s.frames(1000);
        assert(s.deliveries == 0 && s.cancellations == 0);
        s.paused = false; s.frames(5);
        assert(s.deliveries == 0); // reload may begin several frames after close
        s.event(speed == 4 ? "ReloadFast" : "reload"); s.reloading = true;
        s.frames(speed); assert(s.deliveries == 0);
        s.event("reloadStop"); s.frames(3); // end event before graph clears
        assert(s.deliveries == 0);
        s.reloading = false; s.frames(3); assert(s.deliveries == 0);
        s.frames(3); assert(s.deliveries == 1 && s.cancellations == 0);
        s.event("reloadComplete"); s.frames(50); assert(s.deliveries == 1);
    }
    // A completion during EquipObject, before the first update, must be retained.
    { Simulation s; s.event("reload"); s.event("reloadStop"); s.frames(5); assert(s.deliveries == 1); }
    // An event preceding the checkpoint is not evidence for this transaction.
    {
        const auto oldStop = nextReloadSignal(0, ReloadEvent::stop);
        ReloadGate g(oldStop);
        for (int i = 0; i < 10; ++i) assert(g.advance(.05f, true, false, true, false, oldStop) == wait);
    }
    // Replacers without end events use a witnessed graph transition.
    { Simulation s; s.reloading = true; s.frames(60); s.reloading = false; s.frames(5); assert(s.deliveries == 1); }
    // Missing graph variable is okay only with actual end-event evidence.
    { Simulation s; s.reloading.reset(); s.event("reload"); s.frames(60); assert(!s.deliveries);
      s.event("reloadStop"); s.frames(5); assert(s.deliveries == 1); }
    // A start event cannot be ignored during a briefly idle-looking graph.
    { Simulation s; s.event("reloadStart"); s.frames(80); assert(!s.deliveries);
      s.event("reloadStop"); s.frames(5); assert(s.deliveries == 1); }
    // Unknown, stuck, or overlong animations time out by SKIPPING presentation.
    for (const auto state : { -1, 0, 1 }) {
        Simulation s;
        if (state == -1) s.reloading.reset();
        else { s.reloading = state != 0; s.event("reload"); }
        s.frames(400);
        assert(s.deliveries == 0 && s.cancellations == 1);
    }
    // No new reload: known idle still needs a post-menu startup grace period.
    { Simulation s(false); s.frames(19); assert(!s.deliveries); s.frames(8); assert(s.deliveries == 1); }
    // An ammo swap MUST produce completion evidence. An idle-looking first-
    // person graph or missing event cannot become an arbitrary timed fallback.
    { Simulation s; s.frames(400); assert(s.deliveries == 0 && s.cancellations == 1); }
    // An unpaused item menu resets the idle grace period as well.
    { Simulation s(false); s.frames(15); s.paused = true; s.frames(500); s.paused = false;
      s.frames(10); assert(!s.deliveries); s.event("reload"); s.reloading = true; s.frames(40);
      s.event("reloadStop"); s.reloading = false; s.frames(5); assert(s.deliveries == 1); }
    // Another reload begins during the blend-out: wait for the newer one.
    { Simulation s; s.event("reloadStop"); s.frames(2); s.event("reload"); s.frames(40);
      assert(!s.deliveries); s.event("reloadStop"); s.frames(5); assert(s.deliveries == 1); }
    // Asynchronous ammo equip must finish before even a fast reload is released.
    { Simulation s; s.equipped = false; s.event("reloadStop"); s.frames(50); assert(!s.deliveries);
      s.equipped = true; s.frames(5); assert(s.deliveries == 1); }
    // Equipment change, death, sheathing and load-generation invalidation all cancel.
    for (int stage = 0; stage < 3; ++stage) {
        Simulation s;
        if (stage > 0) { s.event("reload"); s.frames(2); }
        if (stage > 1) { s.event("reloadStop"); s.frames(2); }
        s.valid = false; s.paused = true;
        assert(s.tick() == cancel); s.valid = true; s.paused = false; s.frames(400);
        assert(s.deliveries == 0 && s.cancellations == 1);
    }
    // Hitches/invalid deltas cannot jump over the startup/settle guards.
    { Simulation s; s.tick(1000); s.tick(-10); s.tick(std::numeric_limits<float>::quiet_NaN()); assert(!s.deliveries); }
    // Irrelevant animation events do not manufacture reload completion.
    { Simulation s; s.reloading.reset(); s.event("arrowAttach"); s.event("attackStop"); s.frames(400);
      assert(s.deliveries == 0 && s.cancellations == 1); }
    std::cout << "Reload sequencing: menu delay, event/graph completion, varying speeds, cancellation, timeout and exactly-once checks passed\n";
}
