#include "CorpseExplosion.h"
#include "BlastDelivery.h"
#include <cassert>
#include <iostream>

using namespace corpse;
int main()
{
    const Origin oil{true,false,10,Type::fire};
    const Origin chain{true,true,chainSpellLocal(2,0),Type::fire,2};
    // Every exact tier boundary, fractional levels and the upper cap.
    for (const auto [skill,bonus] : std::initializer_list<std::pair<double,double>>{
        {-10,50},{0,50},{25,50},{25.99,50},{26,100},{50,100},{50.99,100},
        {51,150},{75,150},{75.99,150},{76,200},{100,200},{150,200},
        {std::numeric_limits<double>::quiet_NaN(),50},
        {std::numeric_limits<double>::infinity(),50}}) {
        assert(alchemyBonus(skill)==bonus);
        assert(chainAllowance(skill)==bonus/50);
        Ledger tier; tier.record(99,oil,200); assert(tier.killed(99,oil));
        auto result=tier.claim(99,skill); assert(result && result->at(0)==100+bonus);
        assert(!tier.claim(99,skill));
    }
    Ledger ledger;
    // A nominal 5000 poison hit removes 200 Health: first explosion is 200.
    ledger.record(1,oil,healthLost(200,-4800));
    assert(ledger.killed(1,oil));
    auto first=ledger.claim(1,50); assert(first && first->at(0)==200);
    BlastDelivery delivery{1,Type::fire,first->at(0),{2,3,4}};
    const auto hit=delivery.claim(2,0); assert(hit.magnitude==200);
    // Never-oiled enemy with 120 Health dies to the blast: next is 160.
    Frame area{2,120,0,chain}; Frame native{2,120,0,chain,&area};
    const auto inner=native.finish(-80,true); const auto outer=area.finish(-80,true);
    ledger.record(2,chain,inner.damage); ledger.record(2,chain,outer.damage);
    assert(inner.damage==120 && outer.damage==0 && outer.death);
    assert(ledger.killed(2,*outer.death));
    auto second=ledger.claim(2,50); assert(second && second->at(0)==160);
    // Third generation with 100 Health gives 150, retaining the lethal type.
    const Origin lastParent{true,true,chainSpellLocal(1,0),Type::fire,1};
    ledger.record(3,lastParent,healthLost(100,-60)); assert(ledger.killed(3,lastParent));
    assert(ledger.claim(3,50)->at(0)==150);
    for (ID id : {1u,2u,3u}) {
        assert(!ledger.claim(id,50)); ledger.record(id,chain,1000);
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
        assert(!noChain.killed(5,killer)); assert(!noChain.claim(5,50));
    }
    Ledger combined; combined.record(6,weapon,80); combined.record(6,chain,120);
    assert(combined.killed(6,chain)); assert(combined.claim(6,50)->at(0)==200);
    // The flat 100 is one budget, even for mixed original coatings.
    Ledger mixed; mixed.record(7,oil,150);
    mixed.record(7,Origin{true,false,10,Type::poison},50);
    assert(mixed.killed(7,oil)); auto burst=mixed.claim(7,50);
    assert(burst->at(0)==150 && burst->at(3)==50);
    Ledger topTier; topTier.record(7,oil,150);
    topTier.record(7,Origin{true,false,10,Type::poison},50);
    assert(topTier.killed(7,oil)); auto split=topTier.claim(7,100);
    assert(split->at(0)==225 && split->at(3)==75); // One 200 bonus, split 75/25.
    // Every supported type survives chain attribution and a pending save/load.
    for (unsigned i=0;i<4;++i) {
        const Origin typed{true,true,chainSpellLocal(2,i),static_cast<Type>(i),2};
        Ledger pending; pending.record(8,typed,50); assert(pending.killed(8,typed));
        auto loaded=decode(encode(pending),[](ID id){return id+1;});
        assert(loaded); auto result=loaded->claim(9,50); assert(result && result->at(i)==125);
        double sum{};for(auto d:*result)sum+=d;assert(sum==125 && !loaded->claim(9,50));
    }
    // Every tier allows exactly N descendants after the initial oil burst.
    // Branches retain the parent's budget; save/load and changing skill cannot
    // replenish it. The final generation still damages, but creates no burst.
    for (double skill : {0.,25.,26.,50.,51.,75.,76.,100.,150.}) {
        Ledger finite;
        ID victim=100;
        finite.record(victim,oil,100); assert(finite.killed(victim,oil));
        unsigned remaining=99;
        assert(finite.claim(victim,skill,&remaining));
        const auto limit=chainAllowance(skill); assert(remaining==limit);
        for (unsigned depth=1; depth<=limit; ++depth) {
            const auto incoming=remaining;
            const Origin parent{true,true,chainSpellLocal(incoming,0),Type::fire,incoming};
            for (ID sibling : {ID(100+depth*2),ID(101+depth*2)}) {
                finite.record(sibling,parent,100); assert(finite.killed(sibling,parent));
                auto loaded=decode(encode(finite),[](ID id){return id;});
                assert(loaded); finite=std::move(*loaded);
                // Even an Alchemy jump to 1000 cannot reset this branch.
                assert(finite.claim(sibling,1000,&remaining));
                assert(remaining==limit-depth);
            }
        }
        assert(remaining==0);
        const Origin terminal{true,true,chainSpellLocal(0,0),Type::fire,0};
        finite.record(999,Origin{true,true,chainSpellLocal(4,0),Type::fire,4},20);
        finite.record(999,oil,20); // Earlier hits must not reset a terminal killing blow.
        finite.record(999,terminal,60); assert(!finite.killed(999,terminal));
        assert(!finite.claim(999,1000));
        auto loaded=decode(encode(finite),[](ID id){return id;});
        assert(loaded && !loaded->claim(999,1000));
        // A different fresh oil kill is a separate chain with its own budget.
        finite.record(1000,oil,100); assert(finite.killed(1000,oil));
        assert(finite.claim(1000,skill,&remaining) && remaining==limit);
    }
    // Spell identities encode all 20 type/budget pairs without aliases.
    std::map<ID,std::pair<unsigned,unsigned>> identities;
    for (unsigned remaining=0;remaining<=4;++remaining)
        for (unsigned type=0;type<4;++type)
            assert(identities.emplace(chainSpellLocal(remaining,type),std::pair{remaining,type}).second);
    assert(identities.size()==chainSpellCount);
    ledger.resurrected(2); ledger.record(2,chain,50);
    assert(ledger.killed(2,chain) && ledger.claim(2,50)->at(0)==125);
    std::cout << "PASS: Alchemy tier boundaries/cap, finite branched chains, save-stable budgets, typed bonus, actual Health caps, nested hooks, resistance, duplicate deaths and saved pending chains\n";
}
