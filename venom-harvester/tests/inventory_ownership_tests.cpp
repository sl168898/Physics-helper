#include "DeferredForms.h"
#include "InventoryBottleRefs.h"
#include <array>
#include <cassert>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>

namespace
{
    using ID = std::uint32_t;
    constexpr ID poison = 0xFF000DB1;
    struct World
    {
        inline static std::uint64_t epoch = 1;
        inline static std::unordered_map<ID, int> refs, inventory;
        inline static int acquisitions = 0, failAt = -1, releases = 0;
        static std::uint64_t generation() { return epoch; }
        static bool acquire(ID id)
        {
            if (acquisitions++ == failAt || !refs.contains(id)) return false;
            ++refs.at(id);
            return true;
        }
        static void release(ID id)
        {
            assert(refs.contains(id) && refs.at(id) > 0);
            ++releases;
            if (!--refs.at(id)) refs.erase(id);
        }
        static void add(ID id, int count) { inventory[id] += count; } // NO native ref credit.
        static void consume(ID id)
        {
            assert(inventory.at(id) > 0);
            --inventory.at(id);
            release(id); // Normal per-bottle native removal lifecycle.
        }
        static void reset()
        {
            ++epoch;
            refs.clear(); inventory.clear();
            acquisitions = releases = 0; failAt = -1;
        }
    };
    using Events = harvest::DeferredForms<World>;
    using Bottles = harvest::InventoryBottleRefs<World>;

    void inventoryEntryOutlivesObjectWithoutBottleOwnership()
    {
        World::reset();
        World::refs[poison] = 1; // AddPoison's creation smart pointer.
        const std::array<ID, 1> ids{poison};
        auto events = Events::retain(World::epoch, ids);
        World::add(poison, 1);
        World::release(poison); // Creation owner ends.
        events.reset(); // The 2.0.7 cleanup drops the last ref.
        assert(World::inventory.at(poison) == 1 && !World::refs.contains(poison));
        // A later inventory/tooltip lookup now sees a stale object entry.
    }

    void explicitOwnershipSurvivesCleanupAndEndsWithConsumption(int count)
    {
        World::reset();
        World::refs[poison] = 1;
        const std::array<ID, 1> ids{poison};
        std::deque<std::function<void()>> queue;
        auto events = Events::retain(World::epoch, ids);
        auto bottles = Bottles::retain(World::epoch, poison, count);
        assert(events && bottles);
        assert(harvest::retainAcrossQueuedEvents(std::move(events), [&] {
            World::add(poison, count);
            bottles->transferToInventory();
            queue.push_back([&] { assert(World::refs.contains(poison)); });
        }, [&](auto cleanup) { queue.push_back(std::move(cleanup)); }));
        bottles.reset();
        World::release(poison);
        while (!queue.empty()) {
            auto task = std::move(queue.front()); queue.pop_front(); task();
        }
        assert(World::refs.at(poison) == count && World::inventory.at(poison) == count);
        for (int i = count; i > 0; --i) {
            assert(World::refs.at(poison) == i);
            World::consume(poison);
        }
        assert(!World::refs.contains(poison) && World::inventory.at(poison) == 0);
    }

    void failureAndAbandonmentRollBackOnlyReservedRefs()
    {
        World::reset();
        World::refs[poison] = 1;
        World::failAt = 2; // Third of five acquisitions fails.
        assert(!Bottles::retain(World::epoch, poison, 5));
        assert(World::refs.at(poison) == 1 && World::releases == 2 && World::inventory.empty());
        World::failAt = -1;
        { auto bottles = Bottles::retain(World::epoch, poison, 3); assert(bottles); }
        assert(World::refs.at(poison) == 1 && World::inventory.empty());
        assert(!Bottles::retain(World::epoch, poison, 0));
        assert(!Bottles::retain(World::epoch, poison, 100001));
        assert(!Bottles::retain(World::epoch - 1, poison, 1));
        assert(!Bottles::retain(World::epoch, poison + 1, 1));
    }

    void oldReservationCannotReleaseReusedID()
    {
        World::reset();
        World::refs[poison] = 1;
        auto bottles = Bottles::retain(World::epoch, poison, 4);
        assert(bottles);
        World::reset();
        World::refs[poison] = 17;
        bottles.reset();
        assert(World::refs.at(poison) == 17 && World::releases == 0);
    }
}

int main()
{
    inventoryEntryOutlivesObjectWithoutBottleOwnership();
    explicitOwnershipSurvivesCleanupAndEndsWithConsumption(1);
    explicitOwnershipSurvivesCleanupAndEndsWithConsumption(4);
    failureAndAbandonmentRollBackOnlyReservedRefs();
    oldReservationCannotReleaseReusedID();
    std::cout << "Inventory ownership: old dangling-entry reproduction, single/multiple bottles after cleanup, normal consumption, failed acquisition rollback and stale-world safety passed\n";
}
