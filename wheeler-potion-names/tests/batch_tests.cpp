#include "bin/Wheeler/WheelItems/AlchemyBatch.h"
#include <cstdlib>
#include <iostream>

using namespace AlchemyBatch;
static int checks = 0;
static void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}

int main()
{
    const std::vector<Stack> inventory{{"Boss Fight", 3}, {"Travel", 2}, {"Boss Fight", 4}, {"Potion of Health", 5}};
    auto boss = Select(inventory, std::string("Boss Fight"));
    require(boss.count == 7 && boss.firstStack == 0, "Combine only equally named stacks");
    auto travel = Select(inventory, std::string("Travel"));
    require(travel.count == 2 && travel.firstStack == 1, "Select the second named batch");
    auto plain = Select(inventory, std::string("Potion of Health"));
    require(plain.count == 5 && plain.firstStack == 3, "Keep unrenamed copies separate");
    auto missing = Select(inventory, std::string("Gone"));
    require(missing.count == 0 && !missing.firstStack, "Missing batch must not use another name");
    require(Select(inventory, std::nullopt).count == 14, "Old form-only entries retain aggregate behavior");
    require(Select(inventory, std::string("boss fight")).count == 0, "Distinct case must not merge names");

    std::optional<std::string> legacy;
    AdoptUnambiguousName(inventory, "Potion of Health", legacy);
    require(!legacy, "Do not guess the intended name for an ambiguous old slot");
    AdoptUnambiguousName({{"Boss Fight", 2}, {"Boss Fight", 4}}, "Potion of Health", legacy);
    require(legacy == "Boss Fight", "Upgrade an unambiguous old slot");
    AdoptUnambiguousName({{"Travel", 4}}, "Potion of Health", legacy);
    require(legacy == "Boss Fight", "Depletion never switches a bound slot to another batch");
    require(Select({{"Travel", 4}}, legacy).count == 0, "Depleted binding remains unavailable");
    require(Select({{"Travel", 4}, {"Boss Fight", 1}}, legacy).count == 1, "Reacquired batch becomes available");

    legacy.reset();
    AdoptUnambiguousName({{"Potion of Health", 2}}, "Potion of Health", legacy);
    require(!legacy, "Leave ordinary vanilla entries unchanged");
    AdoptUnambiguousName({{"Ghost", 0}, {"精力补给", 2}}, "Potion", legacy);
    require(legacy == "精力补给", "Preserve UTF-8 exactly and ignore empty stacks");
    require(Select({{"精力补给", 2}}, legacy).count == 2, "UTF-8 identity works");
    require(Select({{"Old", -1}, {"Old", 0}}, std::string("Old")).count == 0, "Ignore nonpositive counts");
    require(Select({}, std::nullopt).count == 0, "Empty inventory is unavailable");
    std::cout << checks << " batch-selection checks passed\n";
}
