#pragma once
#include <cmath>

namespace harvest
{
    // kNone is the native "use ValueModifierEffect::actorValue" sentinel.
    // This is for observation only: forward the original argument unchanged.
    template<class ActorValue>
    constexpr ActorValue effectiveActorValue(ActorValue argument, ActorValue stored, ActorValue none)
    {
        return argument == none ? stored : argument;
    }

    // MagicTarget is a secondary subobject (Actor+0xA0 on Skyrim 1.6.1170).
    // Compare like pointers through the runtime accessor; never reinterpret a
    // MagicTarget address as the actor's base address.
    template<class MagicTarget, class Actor>
    bool matchesMagicTarget(const MagicTarget* target, Actor* actor)
    {
        return target && actor && target == actor->AsMagicTarget();
    }

    struct HealthTransition
    {
        bool healthChange{};
        bool aliveBefore{};
        float before{}, after{};
        bool killQueuedBefore{}, killQueuedAfter{};
        bool dyingOrDeadAfter{};
        bool essential{};
    };

    inline bool isLethalHealthChange(const HealthTransition& change)
    {
        if (!change.healthChange || !change.aliveBefore || change.killQueuedBefore ||
            !std::isfinite(change.before) || !std::isfinite(change.after) ||
            change.before <= 0 || change.after > 0) return false;
        // Poison damage can queue death before the life-state changes. Require
        // that THIS operation newly queued it, plus an actual Health crossing.
        // Essential bleedout and merely having poison active cannot qualify.
        return change.dyingOrDeadAfter || (change.killQueuedAfter && !change.essential);
    }
}
