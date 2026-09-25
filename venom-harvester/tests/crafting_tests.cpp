#include "CraftCapture.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace harvest;
constexpr ID defaultPoison = 0x5629E, poison = 0xFF001001, otherPoison = 0xFF001002;
constexpr ID oldBound = 0xFF001003, healing = 0xFF001004;
const Recipe roots{0x100, 0x200};
bool unbound(ID id) { return id == poison || id == otherPoison; }
int main()
{
    {
        // Reproduce the uploaded log: ItemCrafted names DefaultPoison, whose
        // inventory delta is zero, but the actual crafted form gains bottles.
        const ID eventForm = defaultPoison;
        const InventoryCounts before{{0x100, 5}, {0x200, 8}, {poison, 7}};
        const InventoryCounts after{{0x100, 4}, {0x200, 7}, {poison, 10}};
        assert(inventoryCount(after, eventForm) - inventoryCount(before, eventForm) == 0);
        const auto output = identifyCraftOutput(eventForm != 0, before, after, unbound);
        assert(output.status == OutputStatus::found && output.item == poison && output.bottles == 3);
        const auto cost = consumedIngredients(roots, before, after);
        assert(cost == Ingredients({{0x100, 1}, {0x200, 1}}));
        // All three new bottles / multiple weapon hits get one refund budget.
        // The seven pre-existing bottles are not part of the exchange.
        Ledger ledger;
        ledger.sequence = 1;
        assert(ledger.remember(roots));
        assert(ledger.add({oldBound, 1, roots, *cost, false}));
        assert(!ledger.offer(1, poison)); // Original stack remains untracked.
        assert(ledger.offer(1, oldBound));
        assert(ledger.claimVerified(1, [](ID) { return true; }) == cost);
        assert(!ledger.offer(2, oldBound));
    }
    {
        const InventoryCounts before{{poison, 5}};
        const InventoryCounts after{{poison, 6}};
        // Purchases/free refills outside a captured craft cannot be bound.
        assert(identifyCraftOutput(false, before, after, unbound).status == OutputStatus::noCraftEvent);
        assert(identifyCraftOutput(true, before, before, unbound).status == OutputStatus::noOutput);
        assert(identifyCraftOutput(true, {}, {{healing, 1}, {oldBound, 1}}, unbound).status == OutputStatus::noOutput);
        // A second possible poison is ambiguous: do not guess or exchange it.
        assert(identifyCraftOutput(true, {}, {{poison, 1}, {otherPoison, 1}}, unbound).status == OutputStatus::ambiguous);
        const auto onlyPoison = identifyCraftOutput(true, {}, {{poison, 2}, {healing, 1}}, unbound);
        assert(onlyPoison.status == OutputStatus::found && onlyPoison.item == poison && onlyPoison.bottles == 2);
    }
    {
        // A reused native result FormID still binds only newly added bottles.
        const auto output = identifyCraftOutput(true, {{poison, 20}}, {{poison, 21}}, unbound);
        assert(output.status == OutputStatus::found && output.bottles == 1);
        assert(identifyCraftOutput(true, {{poison, 20}}, {{poison, 19}}, unbound).status == OutputStatus::noOutput);
        // A template that really did enter inventory is counted normally too.
        const auto actualTemplate = identifyCraftOutput(true, {}, {{defaultPoison, 1}}, [](ID) { return true; });
        assert(actualTemplate.status == OutputStatus::found && actualTemplate.item == defaultPoison);
    }
    {
        const InventoryCounts before{{0x100, 2}, {0x200, 3}};
        assert(consumedIngredients(roots, before, {{0x100, 1}, {0x200, 3}}) == Ingredients({{0x100, 1}}));
        assert(consumedIngredients(roots, before, before) == Ingredients{});
        assert(!validCost(*consumedIngredients(roots, before, before), roots));
        assert(!consumedIngredients(roots, before, {{0x100, 3}, {0x200, 2}}));
        assert(!consumedIngredients(roots, before, {{0x100, -1}, {0x200, 2}}));
        assert(!consumedIngredients({0x100}, before, {}));
    }
    {
        // Widen before subtraction: corrupt/overflowing counts cannot create
        // phantom output or wrap a negative consumption into a refund.
        constexpr auto maximum = std::numeric_limits<std::int32_t>::max();
        constexpr auto minimum = std::numeric_limits<std::int32_t>::min();
        assert(identifyCraftOutput(true, {{poison, minimum}}, {{poison, maximum}}, unbound).status == OutputStatus::invalidCount);
        assert(identifyCraftOutput(true, {}, {{poison, 100001}}, unbound).status == OutputStatus::invalidCount);
        assert(!consumedIngredients(roots, {{0x100, maximum}, {0x200, 1}}, {{0x100, minimum}}));
    }
    std::cout << "Satchel crafting: DefaultPoison template mismatch, actual output, existing stock, multi-bottle refund, ambiguity, free crafts and count bounds passed\n";
}
