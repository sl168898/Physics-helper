#include "PendingCrafts.h"
#include <cassert>
#include <iostream>

using namespace harvest;
int main()
{
    const std::string oil = "Weapon Oil of Jarrin Crown";
    const std::string frost = "Oil of Greater Frost";
    const NameCounts before{{"",5},{oil,3},{frost,2}};
    auto after = before; after[oil] += 2;
    assert(craftedBatchName(before,after,2) == oil);
    assert(!craftedBatchName(before,after,1));
    after = before; after[""] += 1;
    assert(craftedBatchName(before,after,1) == "");
    after = before; after[oil] += 1; after[frost] += 1;
    assert(!craftedBatchName(before,after,2));
    after = before; after[oil] -= 1; after[frost] += 2;
    assert(!craftedBatchName(before,after,1)); // rename is not a crafted output
    assert(!craftedBatchName(before,before,1));
    assert(!craftedBatchName({},{{oil,0}},0));
    assert(craftedBatchName({},{{oil,1}},1) == oil);
    const std::string unicode = "\xE6\xAF\x92 Weapon Oil";
    assert(craftedBatchName({},{{unicode,1}},1) == unicode);
    assert(!validBatchName(std::string("bad\0name",8)));
    assert(!validBatchName(std::string(maxBatchNameBytes+1,'a')));

    const ID source = 0xFF001234;
    const Recipe recipe{11,22};
    const Ingredients cost{{11,1},{22,1}};
    PendingCrafts pending;
    assert(pending.add({source,1,2,0,recipe,cost,oil,true,3},10));
    assert(pending.add({source,2,1,0,recipe,cost,frost,true,2},12));
    assert(pending.add({source,3,1,0,recipe,cost,oil,true,5},13));
    assert(pending.entries()[2].nameReserve == 3); // excludes three old named bottles
    const auto bytes = encodePending(pending);
    const auto restored = decodePending(bytes,[](ID id){return id;});
    assert(restored && restored->entries().size()==3);
    assert(encodePending(*restored)==bytes);
    assert(restored->entries()[0].customName==oil && restored->entries()[0].nameKnown);
    assert(restored->entries()[1].customName==frost);
    assert(restored->entries()[2].nameReserve==3);
    assert(!pending.add({source,4,1,0,recipe,cost,oil,true,4},14)); // named outputs went missing
    assert(!pending.add({source,4,1,0,recipe,cost,oil,true,15},14));
    assert(!pending.add({source,4,1,0,recipe,cost,oil,false,0},14));
    for (std::size_t i=0;i<bytes.size();++i)
        assert(!decodePending(std::span(bytes).first(i),[](ID id){return id;}));
    auto trailing=bytes;trailing.push_back(0);
    assert(!decodePending(trailing,[](ID id){return id;}));
    // Actual old v1 layout remains readable; unknown names are never invented.
    std::vector<std::uint8_t> legacy;
    auto put=[&](std::uint32_t n){for(int i=0;i<4;++i)legacy.push_back(static_cast<std::uint8_t>(n>>(8*i)));};
    for(auto n:std::vector<std::uint32_t>{1,1,source,1,2,5,2,11,22,2,11,1,22,1})put(n);
    const auto old=decodePending(legacy,[](ID id){return id;});
    assert(old && old->entries().size()==1 && !old->entries()[0].nameKnown);
    assert(old->entries()[0].cost==cost && old->entries()[0].reserve==5);
    auto mapped=decodePending(bytes,[&](ID id){return id==source?0xFF005678:id;});
    assert(mapped && mapped->entries()[0].source==0xFF005678 && mapped->entries()[0].customName==oil);
    std::cout << "Batch names: crafted deltas, mixed names, reserved older bottles, UTF-8, co-save v2 and legacy v1 passed\n";
}
