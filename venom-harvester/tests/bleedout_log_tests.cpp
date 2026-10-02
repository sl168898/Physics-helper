#include "DamageObservation.h"
#include "Harvest.h"
#include "CorpseExplosion.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
    enum class Life { kAlive = 0, kDying = 1, kDead = 2, kEssentialDown = 7, kBleedout = 8 };
    struct Sequence { float before, bleedout, dead; };
    // Three independently missed kills in the supplied 2026-10-02 log. The
    // lethal tick's true starting Health is the preceding tick's final value.
    constexpr std::array<Sequence, 3> sequences{{
        {20.117188f, 3.8125f, -12.4921875f},
        {20.500595f, 8.272079f, -3.9564362f},
        {22.475403f, 2.0945435f, -18.286316f}
    }};

    void replay(const Sequence& observed)
    {
        const harvest::Recipe recipe{0x1BCBC, 0x4DA23, 0x516C8};
        const harvest::Ingredients cost{{0x1BCBC, 1}, {0x4DA23, 1}, {0x516C8, 1}};
        constexpr harvest::ID poison = 0xFF000F33, victim = 0x84701;
        harvest::Ledger satchel;
        satchel.sequence = 1;
        assert(satchel.remember(recipe) && satchel.add({poison, 1, recipe, cost, false}));
        corpse::Ledger explosion;
        const corpse::Origin origin{true, false, poison, corpse::Type::poison};

        // The tick which enters bleedout is NOT lethal and cannot pay early.
        const auto entered = harvest::isLethalHealthChange({true,
            harvest::canObserveHealthBefore(Life::kAlive, false),
            observed.before, observed.bleedout, false, false, false, false});
        assert(!entered && satchel.eligible(poison));
        corpse::Frame first{victim, observed.before, 0, origin};
        auto firstResult = first.finish(observed.bleedout, entered);
        assert(!firstResult.death);
        explosion.record(victim, origin, firstResult.damage);

        // Old code required state 0 and substituted Health=0 for state 8.
        assert(!harvest::isLethalHealthChange({true, false, 0, observed.dead,
            false, true, false, false}));
        const auto lethal = harvest::isLethalHealthChange({true,
            harvest::canObserveHealthBefore(Life::kBleedout, false),
            observed.bleedout, observed.dead, false, true, false, false});
        assert(lethal && satchel.offer(victim, poison));
        auto refund = satchel.claimVerified(victim, [](auto) { return true; });
        assert(refund && *refund == cost);
        assert(!satchel.claimVerified(victim, [](auto) { return true; }));
        assert(!satchel.offer(victim + 1, poison)); // Still one refund per batch.

        corpse::Frame final{victim, observed.bleedout, 0, origin};
        auto result = final.finish(observed.dead, lethal);
        assert(result.death && result.damage == observed.bleedout); // No overkill.
        explosion.record(victim, origin, result.damage);
        assert(explosion.killed(victim, *result.death));
        const auto burst = explosion.claim(victim);
        assert(burst && std::abs(burst->at(3) - 0.50 * observed.before) < 0.00001);
        assert(!explosion.claim(victim));

        // An essential actor dropping into bleedout is not a refundable kill.
        assert(!harvest::canObserveHealthBefore(Life::kBleedout, true));
        assert(!harvest::isLethalHealthChange({true, false, observed.bleedout,
            observed.dead, false, true, false, true}));
        // A corpse tick and a death committed before the tick cannot be stolen.
        assert(!harvest::canObserveHealthBefore(Life::kDying, false));
        assert(!harvest::canObserveHealthBefore(Life::kDead, false));
        assert(!harvest::canObserveHealthBefore(Life::kEssentialDown, true));
        assert(!harvest::isLethalHealthChange({true, true, observed.bleedout,
            observed.dead, true, true, true, false}));
        // A nonlethal bleedout tick, healing, and non-Health damage stay ineligible.
        assert(!harvest::isLethalHealthChange({true, true, 10, 5, false, false, false, false}));
        assert(!harvest::isLethalHealthChange({true, true, 5, 10, false, false, false, false}));
        assert(!harvest::isLethalHealthChange({false, true, 10, -1, false, true, false, false}));
    }
}
int main()
{
    for (const auto& sequence : sequences) replay(sequence);
    std::cout << "PASS: all three logged bleedout transitions, one refund/burst, actual Health caps, essential and corpse-tick exclusions\n";
}
