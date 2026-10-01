#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <set>
#include <type_traits>
#include <vector>

namespace corpse
{
    struct BlastLocation {
        std::uintptr_t cell{}, world{};
        bool exterior{};
        std::array<double, 3> position{};
    };

    template<class Actor>
    BlastLocation blastLocation(Actor* actor)
    {
        const auto cell = actor ? actor->GetParentCell() : nullptr;
        if (!cell) return {};
        const auto position = actor->GetPosition();
        const bool exterior = cell->IsExteriorCell();
        return {reinterpret_cast<std::uintptr_t>(cell),
            exterior ? reinterpret_cast<std::uintptr_t>(actor->GetWorldspace()) : 0,
            exterior, {position.x, position.y, position.z}};
    }

    inline bool withinBlast(const BlastLocation& origin, const BlastLocation& target, double radius)
    {
        if (!origin.cell || !target.cell || origin.exterior != target.exterior ||
            !std::isfinite(radius) || radius <= 0) return false;
        if (origin.exterior) {
            if (!origin.world || origin.world != target.world) return false;
        } else if (origin.cell != target.cell) return false;
        double distance{};
        for (unsigned i = 0; i < 3; ++i) {
            if (!std::isfinite(origin.position[i]) || !std::isfinite(target.position[i])) return false;
            const double delta = target.position[i] - origin.position[i];
            distance += delta * delta;
        }
        return std::isfinite(distance) && distance <= radius * radius;
    }

    // Instantiate this exact collector against ProcessLists in the plugin and
    // against fake process lists in tests. No TES singleton, world-space sky
    // cell lookup, reference-list lock, spell cast or visual spawn is involved.
    // Only handles survive enumeration; callers resolve/recheck before casting.
    template<auto Continue, class Processes, class Actor>
    auto collectBlastActors(Processes* processes, Actor* body, double radius)
    {
        using Handle = std::remove_cvref_t<decltype(body->GetHandle())>;
        std::vector<Handle> handles;
        if (!processes || !body) return handles;
        const auto origin = blastLocation(body);
        std::set<std::uint32_t> seen;
        processes->ForAllActors([&](Actor* actor) {
            if (actor && actor != body && actor->Is3DLoaded() && !actor->IsDisabled() &&
                withinBlast(origin, blastLocation(actor), radius) &&
                seen.insert(actor->GetFormID()).second)
                handles.push_back(actor->GetHandle());
            return Continue;
        });
        return handles;
    }
}
