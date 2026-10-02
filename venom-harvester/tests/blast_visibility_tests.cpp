#include "BlastTargets.h"
#include "BlastDelivery.h"
#include <cassert>
#include <cmath>
#include <iostream>

namespace {
    struct Cell { bool IsExteriorCell() const { return false; } };
    struct Position { float x{}, y{}, z{}; };
    struct Actor {
        corpse::ID id{};
        Cell* cell{};
        Position position{};
        bool hostile = true, alive = true, loaded = true, disabled = false;
        unsigned aiSightCalls{};
        Cell* GetParentCell() const { return cell; }
        void* GetWorldspace() const { return nullptr; }
        Position GetPosition() const { return position; }
        bool Is3DLoaded() const { return loaded; }
        bool IsDisabled() const { return disabled; }
        bool HasLineOfSight(Actor*, bool& result) { ++aiSightCalls; result = false; return false; }
    };
    struct LoggedHit { corpse::ID body, target; double base, resistance, expected; };
    constexpr LoggedHit hits[] {
        {0xFF002870, 0xFF002872, 42.79999923706055, 0, 42.79999923706055},
        {0xFE10D924, 0xFE10DD9B, 73.85004425048828, 0, 73.85004425048828},
        {0xFE10DDA7, 0xFE10DD9B, 49.6197509765625, 0, 49.6197509765625},
        {0xFE10DDA7, 0x00029600, 49.6197509765625, 75, 12.404937744140625},
        {0x00029600, 0xFE10DD9B, 82.42938995361328, 0, 82.42938995361328},
        {0x00029600, 0xFE30AA57, 82.42938995361328, 0, 82.42938995361328},
        {0xFE30AA57, 0xFE10DD9B, 77.2228012084961, 0, 77.2228012084961},
    };
}

int main()
{
    Cell room, otherRoom;
    const auto enemy = [](Actor* a) { return a->hostile && a->alive; };
    // IDs, amounts and resistances are from the user's native 2.1.6 log.
    // Positions are synthetic, in-range fixtures (not measured game positions).
    // Replay each rejected candidate through the exact shared eligibility gate.
    // This checks our filter; native game obstruction/damage needs an in-game test.
    for (const auto& h : hits) {
        Actor body{h.body, &room, {0, 0, 0}, true, false};
        Actor target{h.target, &room, {-100, 0, 0}};
        bool visible = true;
        assert(!target.HasLineOfSight(&body, visible) && !visible);
        target.aiSightCalls = 0;
        assert(corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        assert(target.aiSightCalls == 0); // No recipient AI-perception veto.
        corpse::BlastDelivery cast{h.body, corpse::Type::poison, h.base, {h.target}};
        const auto q = cast.claim(h.target, h.resistance);
        assert(q.result == corpse::BlastDelivery::Result::ready);
        assert(std::abs(q.magnitude - h.expected) < 0.00001);
        assert(cast.claim(h.target, h.resistance).result == corpse::BlastDelivery::Result::duplicate);

        target.hostile = false; // Nonhostile recipients remain excluded.
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        target.hostile = true; target.alive = false;
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        target.alive = true; target.position.x = corpse::radius + 1;
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        target.position.x = 100; target.cell = &otherRoom;
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        target.cell = &room; target.loaded = false;
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        target.loaded = true; target.disabled = true;
        assert(!corpse::eligibleBlastActor(&body, &target, corpse::radius, enemy));
        assert(!corpse::eligibleBlastActor(&body, &body, corpse::radius, enemy));
    }
    std::cout << "PASS: seven logged visibility exclusions now reach the damage gate; resistance, duplicates, hostility, life, range, cell and loaded-state filters retained\n";
}
