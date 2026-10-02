#include "CorpseExplosion.h"
#include "BlastDelivery.h"
#include <cassert>
#include <iostream>

using namespace corpse;
int main()
{
    const Origin oil{true,false,10,Type::fire};
    const Origin chain{true,true,0x21000F83,Type::fire};
    Ledger ledger;
    // A nominal 5000 poison hit removes 200 Health: first explosion is 200.
    ledger.record(1,oil,healthLost(200,-4800));
    assert(ledger.killed(1,oil));
    auto first=ledger.claim(1); assert(first && first->at(0)==200);
    BlastDelivery delivery{1,Type::fire,first->at(0),{2,3,4}};
    const auto hit=delivery.claim(2,0); assert(hit.magnitude==200);
    // Never-oiled enemy with 120 Health dies to the blast: next is 160.
    Frame area{2,120,0,chain}; Frame native{2,120,0,chain,&area};
    const auto inner=native.finish(-80,true); const auto outer=area.finish(-80,true);
    ledger.record(2,chain,inner.damage); ledger.record(2,chain,outer.damage);
    assert(inner.damage==120 && outer.damage==0 && outer.death);
    assert(ledger.killed(2,*outer.death));
    auto second=ledger.claim(2); assert(second && second->at(0)==160);
    // Third generation with 100 Health gives 150, retaining the lethal type.
    ledger.record(3,chain,healthLost(100,-60)); assert(ledger.killed(3,chain));
    assert(ledger.claim(3)->at(0)==150);
    for (ID id : {1u,2u,3u}) {
        assert(!ledger.claim(id)); ledger.record(id,chain,1000);
        assert(!ledger.killed(id,chain)); // Overlapping waves cannot re-explode corpses.
    }
    // Resistance applies to percentage and flat damage together, once.
    assert(delivery.claim(3,50).magnitude==100);
    assert(delivery.claim(4,100).result==BlastDelivery::Result::resisted);
    Ledger immune; immune.record(4,chain,0); assert(!immune.killed(4,chain));
    // Prior player damage contributes, but another killer cannot steal a chain.
    const Origin weapon{true,false,0,Type::poison}, follower{false,false,10,Type::fire};
    for (auto killer : {weapon,follower,Origin{true,true,0,Type::fire}}) {
        Ledger noChain; noChain.record(5,chain,70); noChain.record(5,weapon,30);
        assert(!noChain.killed(5,killer)); assert(!noChain.claim(5));
    }
    Ledger combined; combined.record(6,weapon,80); combined.record(6,chain,120);
    assert(combined.killed(6,chain)); assert(combined.claim(6)->at(0)==200);
    // The flat 100 is one budget, even for mixed original coatings.
    Ledger mixed; mixed.record(7,oil,150);
    mixed.record(7,Origin{true,false,10,Type::poison},50);
    assert(mixed.killed(7,oil)); auto burst=mixed.claim(7);
    assert(burst->at(0)==150 && burst->at(3)==50);
    // Every supported type survives chain attribution and a pending save/load.
    for (unsigned i=0;i<4;++i) {
        const Origin typed{true,true,0x21000F83+2*i,static_cast<Type>(i)};
        Ledger pending; pending.record(8,typed,50); assert(pending.killed(8,typed));
        auto loaded=decode(encode(pending),[](ID id){return id+1;});
        assert(loaded); auto result=loaded->claim(9); assert(result && result->at(i)==125);
        double sum{};for(auto d:*result)sum+=d;assert(sum==125 && !loaded->claim(9));
    }
    ledger.resurrected(2); ledger.record(2,chain,50);
    assert(ledger.killed(2,chain) && ledger.claim(2)->at(0)==125);
    std::cout << "PASS: multigeneration chains, uncoated victims, typed flat bonus, actual Health caps, nested hooks, resistance, duplicate deaths and saved pending chains\n";
}
