#include "DamageObservation.h"
#include "Harvest.h"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>

namespace
{
    enum class AV : std::int32_t { none = -1, health = 24, magicka = 25, stamina = 26 };
    struct MagicTarget { std::uintptr_t vtable{}; };
    struct Actor
    {
        std::array<std::byte, 0xA0> base{};
        MagicTarget magicTarget;
        MagicTarget* AsMagicTarget() { return &magicTarget; }
    };
    static_assert(offsetof(Actor, magicTarget) == 0xA0);

    void nativeDefaultAVWasSilentlyIgnored()
    {
        const AV nativeArgument = AV::none, effectMember = AV::health;
        assert(nativeArgument != AV::health); // 2.0.8 never entered its Health path.
        const auto observed = harvest::effectiveActorValue(nativeArgument, effectMember, AV::none);
        assert(observed == AV::health);
        assert(nativeArgument == AV::none); // Forward this original to Skyrim.
        assert(harvest::effectiveActorValue(AV::none, AV::stamina, AV::none) == AV::stamina);
        assert(harvest::effectiveActorValue(AV::magicka, AV::health, AV::none) == AV::magicka);
        assert(harvest::effectiveActorValue(AV::health, AV::stamina, AV::none) == AV::health);
        assert(harvest::effectiveActorValue(AV::none, AV::none, AV::none) == AV::none);
    }

    void secondaryTargetAddressWasAlwaysRejected()
    {
        Actor victim, other;
        auto target = victim.AsMagicTarget();
        // Reproduce the pinned library's reinterpret_cast, WITHOUT dereferencing
        // the invalid Actor pointer. It points 0xA0 into the real victim.
        assert(reinterpret_cast<Actor*>(target) != &victim);
        assert(harvest::matchesMagicTarget(target, &victim));
        assert(!harvest::matchesMagicTarget(target, &other));
        assert(!harvest::matchesMagicTarget(static_cast<MagicTarget*>(nullptr), &victim));
        assert(!harvest::matchesMagicTarget(target, static_cast<Actor*>(nullptr)));
    }

    void actualHealthCrossingAndCommittedDeathAreRequired()
    {
        using harvest::HealthTransition;
        using harvest::isLethalHealthChange;
        const HealthTransition immediate{true, true, 100, 0, false, false, true, false};
        const HealthTransition queued{true, true, 100, -10, false, true, false, false};
        assert(isLethalHealthChange(immediate));
        assert(isLethalHealthChange(queued)); // Life state may still be kAlive.

        auto check = queued;
        check.healthChange = false; assert(!isLethalHealthChange(check)); // Stamina damage.
        check = queued; check.after = 1; assert(!isLethalHealthChange(check)); // Poison did not kill.
        check = queued; check.after = 200; assert(!isLethalHealthChange(check)); // Healing.
        check = queued; check.killQueuedAfter = false; assert(!isLethalHealthChange(check)); // No death proof.
        check = queued; check.killQueuedBefore = true; assert(!isLethalHealthChange(check)); // Already committed.
        check = queued; check.aliveBefore = false; assert(!isLethalHealthChange(check)); // Corpse tick.
        check = queued; check.before = 0; assert(!isLethalHealthChange(check));
        check = queued; check.essential = true; assert(!isLethalHealthChange(check)); // Essential bleedout.
        check = queued; check.after = std::numeric_limits<float>::quiet_NaN(); assert(!isLethalHealthChange(check));
        check = queued; check.before = std::numeric_limits<float>::infinity(); assert(!isLethalHealthChange(check));
    }

    void implicitHealthQueuedKillReachesExactlyOneBatchRefund()
    {
        constexpr harvest::ID poison = 0xFF000C34, victimID = 0x12345;
        Actor victim;
        const harvest::Recipe recipe{0x1BCBC, 0x4DA23, 0xEC870};
        const harvest::Ingredients cost{{0x1BCBC, 1}, {0x4DA23, 1}, {0xEC870, 1}};
        harvest::Ledger ledger;
        ledger.sequence = 1;
        assert(ledger.remember(recipe));
        assert(ledger.add({poison, 1, recipe, cost, false}));
        const auto av = harvest::effectiveActorValue(AV::none, AV::health, AV::none);
        const bool correctTarget = harvest::matchesMagicTarget(victim.AsMagicTarget(), &victim);
        const bool lethal = harvest::isLethalHealthChange({av == AV::health, true, 200, -1887,
            false, true, false, false});
        assert(correctTarget && lethal && ledger.eligible(poison));
        assert(ledger.offer(victimID, poison));
        const auto refund = ledger.claimVerified(victimID, [](auto) { return true; });
        assert(refund && *refund == cost);
        assert(!ledger.offer(victimID + 1, poison)); // Other bottles/hits cannot multiply it.
        assert(!ledger.claimVerified(victimID, [](auto) { return true; }));
    }
}

int main()
{
    nativeDefaultAVWasSilentlyIgnored();
    secondaryTargetAddressWasAlwaysRejected();
    actualHealthCrossingAndCommittedDeathAreRequired();
    implicitHealthQueuedKillReachesExactlyOneBatchRefund();
    std::cout << "Damage observation: native kNone Health, explicit AV precedence, secondary target address, immediate/queued death, nonlethal/essential rejection and one batch refund passed\n";
}
