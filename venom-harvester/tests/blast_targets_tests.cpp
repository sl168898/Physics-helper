#include "BlastTargets.h"
#include <cassert>
#include <iostream>
#include <limits>

namespace {
    struct Cell {
        bool exterior{};
        bool IsExteriorCell() const { return exterior; }
    };
    struct World {};
    struct Position { float x{}, y{}, z{}; };
    struct Actor {
        std::uint32_t id{};
        Cell* cell{};
        World* world{};
        Position position{};
        bool loaded = true, disabled = false;
        Cell* GetParentCell() const { return cell; }
        World* GetWorldspace() const { return world; }
        Position GetPosition() const { return position; }
        bool Is3DLoaded() const { return loaded; }
        bool IsDisabled() const { return disabled; }
        std::uint32_t GetFormID() const { return id; }
        std::uint32_t GetHandle() const { return id; }
    };
    enum class Result { next };
    struct Processes {
        std::array<std::vector<Actor*>, 4> levels;
        unsigned enumerations{};
        template<class Callback>
        void ForAllActors(Callback callback) {
            ++enumerations;
            for (const auto& level : levels) for (auto actor : level)
                assert(callback(actor) == Result::next);
        }
    };
}

int main()
{
    World tamriel, otherWorld;
    Cell exteriorA{true}, exteriorB{true}, interiorA, interiorB;
    // An exterior corpse near a cell edge must reach enemies in both cells
    // without accessing a global TES world-space or sky-cell pointer.
    Actor body{1, &exteriorA, &tamriel, {4090, 0, 0}};
    Actor adjacent{2, &exteriorB, &tamriel, {4110, 0, 0}};
    Actor boundary{3, &exteriorA, &tamriel, {4090, 0, 420}};
    Actor tooFar{4, &exteriorA, &tamriel, {4090, 0, 420.01f}};
    Actor elsewhere{5, &exteriorB, &otherWorld, {4110, 0, 0}};
    Actor inside{6, &interiorA, nullptr, {4110, 0, 0}};
    Actor unloaded{7, &exteriorA, &tamriel, {4110, 0, 0}, false};
    Actor disabled{8, &exteriorA, &tamriel, {4110, 0, 0}, true, true};
    Actor detached{9, nullptr, &tamriel, {4110, 0, 0}};
    Actor missingWorld{10, &exteriorA, nullptr, {4110, 0, 0}};
    Processes processes;
    processes.levels[0] = {&body, &adjacent, &tooFar, nullptr};
    processes.levels[1] = {&unloaded, &disabled, &detached};
    processes.levels[2] = {&boundary, &adjacent, &elsewhere};
    processes.levels[3] = {&inside, &missingWorld};
    const auto candidates = corpse::collectBlastActors<Result::next>(&processes, &body, 420);
    assert((candidates == std::vector<std::uint32_t>{2, 3}));
    assert(processes.enumerations == 1); // All four process levels, deduplicated.

    // Equal coordinates in different interiors are not adjacent in the game.
    body.cell = &interiorA; body.world = nullptr;
    Actor sameRoom{11, &interiorA, nullptr, {4110, 0, 0}};
    Actor otherRoom{12, &interiorB, nullptr, {4110, 0, 0}};
    processes.levels = {};
    processes.levels[0] = {&sameRoom, &otherRoom, &adjacent};
    assert((corpse::collectBlastActors<Result::next>(&processes, &body, 420) ==
        std::vector<std::uint32_t>{11}));

    // Recheck after the snapshot: a teleport, detachment or move out of range
    // must make a previously collected target ineligible for delivery.
    const auto origin = corpse::blastLocation(&body);
    sameRoom.position.z = 500;
    assert(!corpse::withinBlast(origin, corpse::blastLocation(&sameRoom), 420));
    sameRoom.position.z = 0; sameRoom.cell = &interiorB;
    assert(!corpse::withinBlast(origin, corpse::blastLocation(&sameRoom), 420));
    sameRoom.cell = nullptr;
    assert(!corpse::withinBlast(origin, corpse::blastLocation(&sameRoom), 420));

    auto invalid = origin;
    invalid.position[0] = std::numeric_limits<double>::quiet_NaN();
    assert(!corpse::withinBlast(origin, invalid, 420));
    invalid.position[0] = std::numeric_limits<double>::infinity();
    assert(!corpse::withinBlast(origin, invalid, 420));
    assert(!corpse::withinBlast(origin, origin, 0));
    assert(!corpse::withinBlast(origin, origin, -1));
    assert(!corpse::withinBlast(origin, origin, std::numeric_limits<double>::infinity()));
    assert(corpse::collectBlastActors<Result::next>(static_cast<Processes*>(nullptr), &body, 420).empty());
    assert(corpse::collectBlastActors<Result::next>(&processes, static_cast<Actor*>(nullptr), 420).empty());
    std::cout << "PASS: loaded actor collector, exterior cell boundary, 3D radius, separate spaces, deduplication and delivery rechecks\n";
}
