#include "PendingCrafts.h"
#include <cassert>
#include <iostream>

using namespace harvest;

int main()
{
    constexpr ID source = 0xFF00089C, unique1 = 0xFF001773, unique2 = 0xFF001774;
    const Recipe recipe{0x1BCBC, 0x4DA23, 0xEC870};
    const Ingredients cost{{recipe[0], 1}, {recipe[1], 1}, {recipe[2], 1}};

    // The reported failure: a second brew revisits the original item while
    // CIE still owns borrowed entries. Neither brew may replace it in-menu.
    PendingCrafts queue;
    assert(queue.add({source, 1, 2, 0, recipe, cost}, 5)); // five older bottles
    int nativeCopies = 0, sourceCount = 7;
    auto commit = [&](bool menuExists) {
        auto work = queue.takeWhenClosed(menuExists);
        for (const auto& [id, needed] : requiredSourceCounts(work)) {
            assert(id == source && needed <= static_cast<std::uint64_t>(sourceCount));
        }
        for (const auto& entry : work) {
            ++nativeCopies;
            sourceCount -= entry.bottles;
        }
        return work;
    };
    assert(commit(true).empty());
    assert(nativeCopies == 0 && sourceCount == 7);
    assert(queue.add({source, 2, 1, 0, recipe, cost}, sourceCount));
    ++sourceCount;
    assert(commit(true).empty()); // closing animation / quick reopen / still at station
    assert(nativeCopies == 0 && sourceCount == 8);

    // A save in the open menu preserves exact expenditure and separate batch
    // identities. Dynamic source/ingredient IDs resolve through the co-save API.
    const auto bytes = encodePending(queue);
    auto restored = decodePending(bytes, [](ID id) { return id; });
    assert(restored && restored->entries().size() == 2);
    assert(encodePending(*restored) == bytes);
    const auto mapped = decodePending(bytes, [](ID id) { return id == source ? 0xFF000AAA : id; });
    assert(mapped && mapped->entries()[0].source == 0xFF000AAA);
    const auto absent = decodePending(bytes, [](ID id) { return id == source ? 0 : id; });
    assert(absent && absent->empty());
    for (std::size_t n = 0; n < bytes.size(); ++n)
        assert(!decodePending(std::span(bytes).first(n), [](ID id) { return id; }));
    auto extra = bytes; extra.push_back(0);
    assert(!decodePending(extra, [](ID id) { return id; }));

    const auto completed = commit(false);
    assert(completed.size() == 2 && nativeCopies == 2 && sourceCount == 5);
    assert(queue.empty() && commit(false).empty()); // cannot bind twice
    Ledger ledger;
    ledger.sequence = 2; ledger.stored = recipe;
    for (std::size_t i = 0; i < completed.size(); ++i) {
        const auto& entry = completed[i];
        assert(ledger.add({i == 0 ? unique1 : unique2, entry.nonce, entry.recipe, entry.cost, false}));
    }
    assert(ledger.offer(100, unique1) && ledger.claim(100) == cost);
    assert(!ledger.offer(101, unique1)); // second bottle/hit/victim shares budget
    assert(ledger.offer(102, unique2) && ledger.claim(102) == cost);
    assert(!ledger.offer(103, source)); // older/purchased bottles still excluded

    // Missing output must never consume the five reserved older bottles.
    const auto needed = requiredSourceCounts(restored->entries());
    assert(needed.at(source) == 8);
    assert(7 < needed.at(source));
    assert(!restored->add({source, 3, 1, 0, recipe, cost}, 7));
    assert(!restored->add({source, 2, 1, 0, recipe, cost}, 8)); // duplicate nonce
    assert(!restored->add({source, 3, 0, 0, recipe, cost}, 8));
    assert(!restored->add({source, 3, 1, 0, recipe, {}}, 8)); // free craft
    assert(!restored->add({source, 3, 1, 0, recipe, cost}, INT32_MAX));
    restored->clear();
    assert(restored->empty()); // save transition cancels previous-world work
    assert(decodePending(encodePending(*restored), [](ID id) { return id; })->empty());
    std::cout << "Pending crafts: menu lifetime, repeated brews, reserved inventory, save/load and per-batch refunds passed\n";
}
