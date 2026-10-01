#include "DeferredForms.h"
#include "InventoryBottleRefs.h"
#include <array>
#include <cassert>
#include <deque>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace
{
    using ID = std::uint32_t;
    constexpr ID source = 0xFF000C13, first = 0xFF000C99, second = 0xFF000C9A;

    struct Forms
    {
        inline static std::uint64_t world = 1;
        inline static std::unordered_map<ID, int> refs;
        inline static std::vector<ID> releases;
        static std::uint64_t generation() { return world; }
        static bool acquire(ID id)
        {
            auto it = refs.find(id);
            if (it == refs.end()) return false;
            ++it->second;
            return true;
        }
        static void release(ID id)
        {
            auto it = refs.find(id);
            assert(it != refs.end() && it->second > 0);
            releases.push_back(id);
            if (--it->second == 0) refs.erase(it);
        }
        static bool exists(ID id) { return refs.contains(id); }
        static void reset()
        {
            ++world;
            refs.clear();
            releases.clear();
        }
    };
    using Lease = harvest::DeferredForms<Forms>;
    using Bottles = harvest::InventoryBottleRefs<Forms>;

    // Raw inventory entries and created-object ownership are separate. The old
    // test incorrectly granted a native ref as a side effect of inventory add.
    std::unordered_map<ID, int> inventory;
    void addObjectToContainer(ID id, int count) { inventory[id] += count; }

    struct Queue
    {
        std::deque<std::function<void()>> tasks;
        void push(std::function<void()> task) { tasks.push_back(std::move(task)); }
        void runOne()
        {
            assert(!tasks.empty());
            auto task = std::move(tasks.front());
            tasks.pop_front();
            task();
            // Task disposal releases captures, as SKSE does after Run().
        }
        void drain() { while (!tasks.empty()) runOne(); }
    };

    void unretainedSwapReproducesMissingForm()
    {
        Forms::reset();
        Forms::refs[source] = 1; // The one crafted bottle.
        Queue queue;
        bool missing = false;
        queue.push([&] { missing = !Forms::exists(source); }); // PAPER added event.
        Forms::release(source); // Replacing the last bottle destroys its form.
        queue.drain();
        assert(missing); // The null lookup seen in the user's crash report.
    }

    void exchangeKeepsBothFormsThroughEvents()
    {
        Forms::reset();
        inventory.clear();
        Forms::refs[source] = 1; // Inventory.
        Forms::refs[first] = 1;  // Native creation's scoped owner.
        Queue queue;
        int delivered = 0;
        queue.push([&] { assert(Forms::exists(source)); ++delivered; });
        const std::array<ID, 2> ids{source, first};
        auto lease = Lease::retain(Forms::world, ids);
        assert(lease);
        auto bottles = Bottles::retain(Forms::world, first, 1);
        assert(bottles);
        assert(harvest::retainAcrossQueuedEvents(std::move(lease), [&] {
            Forms::release(source);
            queue.push([&] { assert(Forms::exists(source)); ++delivered; });
            addObjectToContainer(first, 1);
            bottles->transferToInventory();
            queue.push([&] { assert(Forms::exists(first)); ++delivered; });
        }, [&](auto task) { queue.push(std::move(task)); }));
        Forms::release(first); // makeBatch's scoped smart pointer leaves scope.
        assert(queue.tasks.size() == 4);
        for (int i = 0; i < 3; ++i) {
            queue.runOne();
            assert(Forms::exists(source) && Forms::exists(first));
        }
        assert(delivered == 3);
        queue.runOne(); // Cleanup follows all added/removed event consumers.
        assert(!Forms::exists(source)); // No permanent pin or leaked source form.
        assert(Forms::refs.at(first) == 1); // Only the real inventory owns it.
        assert(inventory.at(first) == 1);
        bottles.reset();
        assert(Forms::refs.at(first) == 1); // Transfer is not an extra plugin pin.
    }

    void overlappingCraftsKeepIndependentReferences()
    {
        Forms::reset();
        inventory.clear();
        Forms::refs[source] = 2;
        Forms::refs[first] = Forms::refs[second] = 1;
        Queue queue;
        int delivered = 0;
        for (ID replacement : {first, second}) {
            queue.push([&] { assert(Forms::exists(source)); ++delivered; });
            const std::array<ID, 2> ids{source, replacement};
            auto bottles = Bottles::retain(Forms::world, replacement, 1);
            assert(bottles);
            assert(harvest::retainAcrossQueuedEvents(Lease::retain(Forms::world, ids), [&] {
                Forms::release(source);
                queue.push([&] { assert(Forms::exists(source)); ++delivered; });
                addObjectToContainer(replacement, 1);
                bottles->transferToInventory();
                queue.push([&, replacement] { assert(Forms::exists(replacement)); ++delivered; });
            }, [&](auto task) { queue.push(std::move(task)); }));
            Forms::release(replacement);
        }
        for (int i = 0; i < 4; ++i) queue.runOne();
        assert(Forms::refs.at(source) == 1); // The second exchange still owns it.
        queue.drain();
        assert(delivered == 6 && !Forms::exists(source));
        assert(Forms::refs.at(first) == 1 && Forms::refs.at(second) == 1);
    }

    void failedRetentionDoesNotMutateOrLeak()
    {
        Forms::reset();
        Forms::refs[source] = 1;
        const std::array<ID, 2> ids{source, first}; // Replacement missing.
        auto lease = Lease::retain(Forms::world, ids);
        assert(!lease && Forms::refs.at(source) == 1);
        bool mutated = false, queued = false;
        assert(!harvest::retainAcrossQueuedEvents(std::move(lease), [&] { mutated = true; },
            [&](auto) { queued = true; }));
        assert(!mutated && !queued);

        const std::array<ID, 2> duplicate{source, source};
        lease = Lease::retain(Forms::world, duplicate);
        assert(lease && Forms::refs.at(source) == 2);
        lease.reset();
        assert(Forms::refs.at(source) == 1); // One pin per distinct form.
        assert(!Lease::retain(Forms::world - 1, duplicate));
    }

    void oldCleanupCannotReleaseReusedIDsAfterLoad()
    {
        Forms::reset();
        Forms::refs[source] = Forms::refs[first] = 1;
        Queue queue;
        const std::array<ID, 2> ids{source, first};
        assert(harvest::retainAcrossQueuedEvents(Lease::retain(Forms::world, ids), [] {},
            [&](auto task) { queue.push(std::move(task)); }));
        Forms::reset(); // Old native forms destroyed; the save epoch advances.
        Forms::refs[source] = 17;
        Forms::refs[first] = 23; // A new world may reuse the same dynamic IDs.
        queue.drain();
        assert(Forms::releases.empty());
        assert(Forms::refs.at(source) == 17 && Forms::refs.at(first) == 23);
    }
}

int main()
{
    unretainedSwapReproducesMissingForm();
    exchangeKeepsBothFormsThroughEvents();
    overlappingCraftsKeepIndependentReferences();
    failedRetentionDoesNotMutateOrLeak();
    oldCleanupCannotReleaseReusedIDsAfterLoad();
    std::cout << "Inventory event lifetime: missing-form reproduction, FIFO retention, overlapping crafts, rollback, no permanent pins and stale-load cleanup passed\n";
}
