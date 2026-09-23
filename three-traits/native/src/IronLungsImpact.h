#pragma once
#include "IronLungs.h"
#include <memory>
#include <utility>

namespace traits {
// Identity belongs to one actual projectile/target callback, never to the
// most recent shout. The cast owns its pre-payment snapshot and target set.
struct IronLungsImpact {
    std::shared_ptr<IronLungsCast> cast;
    std::uint32_t spell = 0, caster = 0, target = 0;

    bool matches(std::uint32_t hitSpell, std::uint32_t hitCaster,
                 std::uint32_t hitTarget, bool accepted, bool applyingBonus) const {
        return cast && spell && caster && target && accepted && !applyingBonus &&
            spell == hitSpell && caster == hitCaster && target == hitTarget;
    }
};

class IronLungsImpactScope {
    IronLungsImpact& slot;
    IronLungsImpact previous;
public:
    IronLungsImpactScope(IronLungsImpact& value, IronLungsImpact current) :
        slot(value), previous(std::exchange(value, std::move(current))) {}
    ~IronLungsImpactScope() { slot = std::move(previous); }
    IronLungsImpactScope(const IronLungsImpactScope&) = delete;
    IronLungsImpactScope& operator=(const IronLungsImpactScope&) = delete;
};
}
