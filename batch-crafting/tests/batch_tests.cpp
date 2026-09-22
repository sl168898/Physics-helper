#include "Batch.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

// A container-first removal chain, modelling SCIE's published RemoveItem hook.
// The native recipe calls this once per ingredient, not once per crafted item.
struct Inventory {
    std::vector<std::map<std::uint32_t,std::int64_t>> containers;
    std::map<std::uint32_t,std::int64_t> player;
    std::int64_t count(std::uint32_t id) {
        auto n=player[id];for(auto& c:containers)n+=c[id];return n;
    }
    void remove(std::uint32_t id,std::int32_t n) {
        for(auto& c:containers){auto take=std::min<std::int64_t>(n,c[id]);c[id]-=take;n-=static_cast<std::int32_t>(take);}
        assert(player[id]>=n);player[id]-=n;
    }
};
int main() {
    using namespace batch;
    assert(maximum({{1,2,25},{2,1,8}},9,1)==8);
    assert(maximum({{1,2,25},{1,3,25}},9,1)==5);
    assert(maximum({{1,2,25},{1,3,24}},9,1)==0);
    assert(maximum({},9,1)==0);assert(maximum({{1,0,999}},9,1)==0);
    assert(maximum({{9,1,25}},9,1)==0);assert(maximum({{1,1,-1}},9,1)==0);
    assert(maximum({{1,1,9999999}},9,1)==limit);
    assert(maximum({{1,engineLimit,engineLimit}},9,1)==1);
    assert(maximum({{1,engineLimit,engineLimit},{1,1,engineLimit}},9,1)==0);
    assert(maximum({{1,1,99}},9,engineLimit)==1);
    Selection selected{2,100,200,17,9,24};
    // No ingredients in pockets; all materials split between two SCIE sources.
    Inventory inventory{{{{1,7},{2,4}},{{1,13},{2,6}}},{{9,5}}};
    std::vector<Material> materials{{1,2,inventory.count(1)},{2,1,inventory.count(2)}};
    assert(maximum(materials,9,24)==10);
    auto planned=Transaction::create(selected,10,materials);assert(planned);
    assert(!Transaction::create(selected,11,materials));assert(!Transaction::create(selected,0,materials));
    auto& t=*planned;
    Transaction* active=nullptr;
    int nativeCalls=0,removeCalls=0,addCalls=0,skillCalls=0;
    float xp=0;
    auto nativeCraft=[&] {
        ++nativeCalls;
        for(auto [id,cost]:std::map<std::uint32_t,std::int32_t>{{1,2},{2,1}}) {
            const auto n=active->removal(id,cost);
            Scope<Transaction> suspend(active,nullptr);
            assert(active==nullptr);inventory.remove(id,n);++removeCalls;
        }
        const auto n=active->addition(9,24);
        {Scope<Transaction> suspend(active,nullptr);inventory.player[9]+=n;++addCalls;}
        const auto points=active->experience(7.5f);
        {Scope<Transaction> suspend(active,nullptr);assert(!active);xp+=points;++skillCalls;}
    };
    {Scope<Transaction> scope(active,&t);nativeCraft();}
    assert(!active && nativeCalls==1 && removeCalls==2 && addCalls==1 && skillCalls==1);
    assert(t.accounted() && t.totalOutput()==240 && inventory.player[9]==245);
    assert(inventory.count(1)==0 && inventory.count(2)==0 && xp==75.0f);
    // A mixed player/container total is counted once; quantity cannot exceed it.
    Inventory mixed{{{{1,3}}},{{1,7}}};
    assert(maximum({{1,2,mixed.count(1)}},9,1)==5);
    auto mixedPlan=Transaction::create({1,1,1,2,9,1},5,{{1,2,mixed.count(1)}});assert(mixedPlan);
    mixed.remove(1,mixedPlan->removal(1,2));assert(mixed.count(1)==0);
    // Duplicate CNTO records may yield several removal calls for the same form.
    auto duplicate=Transaction::create(selected,2,{{1,2,10},{1,3,10}});assert(duplicate);
    assert(duplicate->removal(1,2)==4 && duplicate->removal(1,3)==6);
    assert(duplicate->addition(9,24)==48 && duplicate->accounted());
    // Unrelated calls, exhausted quotas and nested callbacks never scale again.
    assert(duplicate->removal(1,5)==5 && duplicate->removal(777,2)==2);
    assert(duplicate->addition(9,24)==24 && duplicate->addition(777,2)==2);
    auto missing=Transaction::create(selected,1,{{1,1,1}});assert(missing && !missing->accounted());
    missing->addition(9,24);assert(!missing->accounted());
    // Scope restoration on exceptions prevents the multiplier leaking to later play.
    try {Scope<Transaction> outer(active,&t);Scope<Transaction> inner(active,nullptr);throw std::runtime_error("test");}
    catch(const std::runtime_error&) {}
    assert(active==nullptr);
    auto changed=selected;++changed.session;assert(changed!=selected);
    changed=selected;++changed.recipe;assert(changed!=selected);
    std::cout<<"Passed: one native operation; container-only/mixed sources; aggregate debit/output/XP; duplicate costs; bounds; callback isolation; scope restoration\n";
}
