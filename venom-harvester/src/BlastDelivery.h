#pragma once
#include "CorpseExplosion.h"
#include <set>

namespace corpse
{
    // One native Self-area cast, originating at the corpse. The engine may
    // offer extra actors or the same actor more than once. Only the snapshot
    // of eligible enemies may receive this portion, once per damage type.
    struct BlastDelivery {
        ID victim{};
        Type type{Type::poison};
        double base{};
        std::set<ID> allowed, attempted;
        unsigned accepted{};

        enum class Result { ready, excluded, duplicate, resisted, invalid };
        struct Quote { Result result; float magnitude{}; };

        Quote claim(ID target, double resistance) {
            if (!target || target == victim || !allowed.contains(target)) return {Result::excluded};
            if (!attempted.insert(target).second) return {Result::duplicate};
            if (!std::isfinite(base) || base <= 0 || !std::isfinite(resistance)) return {Result::invalid};
            const double amount = base * resistanceMultiplier(resistance);
            if (!std::isfinite(amount) || amount > std::numeric_limits<float>::max()) return {Result::invalid};
            if (amount <= 0) return {Result::resisted};
            return {Result::ready, static_cast<float>(amount)};
        }
    };
}
