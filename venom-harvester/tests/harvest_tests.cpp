#include "Harvest.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace harvest;
    float magnitude = 100, duration = 20;
    weaken(magnitude, duration, false);
    assert(magnitude == 75 && duration == 20);
    magnitude = 0;
    weaken(magnitude, duration, true);
    assert(duration == 15 && magnitude == 0);
    assert(live(false, false, false, 9, 10, false));
    assert(!live(false, false, false, 10, 10, false));
    assert(live(false, false, false, 10, 10, true));
    assert(!live(false, false, false, 11, 10, true));
    assert(!live(false, false, false, 0, 0, false));
    assert(live(false, false, false, 0, 0, true));
    assert(!live(true, false, false, 0, 10, true));
    assert(!live(false, true, false, 0, 10, true));
    assert(!live(false, false, true, 0, 10, true));
    assert(!live(false, false, false, 0, std::numeric_limits<float>::quiet_NaN(), false));

    const Key a{100, 200, 300, 1}, b{100, 201, 301, 2}, secondEffect{100, 200, 302, 3};
    Ledger l;
    l.begin(a, 200, true); l.begin(secondEffect, 200, true);
    const std::vector<Key> samePoison{a, secondEffect};
    assert(l.death(100, true, samePoison));
    assert(l.claim(100) == 200);
    assert(!l.claim(100));
    l.begin(a, 200, true);                      // resurrected corpse
    assert(!l.death(100, true, samePoison));

    l.clear(); l.begin(a, 200, true); l.begin(b, 201, true);
    const std::vector<Key> mixed{a, b};
    assert(l.death(100, true, mixed));
    assert(l.claim(100) == 201);                // newest still-active poison
    l.clear(); l.begin(a, 200, true); l.begin(b, 201, true); l.end(b, false);
    assert(l.death(100, true, mixed));
    assert(l.claim(100) == 200);                // newest expired; older active poison wins
    l.clear(); l.begin(a, 200, true); l.end(a, false);
    assert(!l.death(100, true, samePoison));     // expired or dispelled
    l.clear(); l.begin(a, 200, true);
    assert(!l.death(100, false, samePoison));    // follower/NPC/environment kill
    assert(!l.claim(100));

    l.clear(); l.begin(a, 0xFF001234, true);
    l.begin(a, 201, false);                    // phial changed after poisoning
    assert(l.find(a)->bottle == 0xFF001234);    // original brewed poison retained
    l.end(a, true);                            // death cleans effects before death event
    assert(!l.claim(100));                     // still needs killer confirmation
    assert(l.death(100, true, {}));
    const auto pendingSave = encode(l);
    auto restored = decode(pendingSave, [](ID id) { return id; });
    assert(restored && restored->claim(100) == 0xFF001234);
    auto done = decode(encode(*restored), [](ID id) { return id; });
    assert(done && !done->claim(100) && done->rewarded.contains(100));

    l.clear(); l.begin(a, 200, true); l.begin(b, 201, true);
    const auto saved = encode(l);
    auto remapped = decode(saved, [](ID id) { return id + 1000; });
    const std::vector<Key> moved{{1100,1200,1300,1},{1100,1201,1301,2}};
    assert(remapped && remapped->death(1100, true, moved));
    assert(remapped->claim(1100) == 1201);
    auto missing = decode(saved, [](ID id) { return id == 201 ? 0 : id; });
    assert(missing && missing->effects.size() == 1);
    for (std::size_t size = 0; size < saved.size(); ++size)
        assert(!decode(std::span(saved).first(size), [](ID id) { return id; }));
    auto corrupt = saved; corrupt.push_back(0);
    assert(!decode(corrupt, [](ID id) { return id; }));
    l.forget(100); assert(l.effects.empty() && l.candidates.empty());
    l.clear(); assert(l.rewarded.empty());      // no state crossing characters
}
