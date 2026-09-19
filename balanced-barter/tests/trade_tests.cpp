#include "TradeRules.h"
#include <cassert>
#include <iostream>
#include <numeric>
#include <array>
using namespace barter;
struct Mock {
    bool valid = true, paymentOK = true;
    int failAt = -1, playerCash = 0, merchantCash = 100, paid = 0;
    std::array<int,4> player{1,0,1,0}, merchant{0,1,0,1};
    std::vector<std::size_t> undone;
    bool validate() { return valid; }
    bool move(std::size_t i) { if (int(i)==failAt) return false; (i%2 ? merchant[i] : player[i])--; (i%2 ? player[i] : merchant[i])++; return true; }
    void undo(std::size_t i) { undone.push_back(i); (i%2 ? player[i] : merchant[i])--; (i%2 ? merchant[i] : player[i])++; }
    void recoverFailed(std::size_t) {}
    bool pay() { if(!paymentOK) return false; ++paid; return true; }
    void refund() {}
};
int main() {
    unsigned cases=0;
    auto check=[&](auto lines, long long p, long long v, Error e) { auto s=evaluate(lines,p,v); assert(s.error==e); ++cases; return s; };
    std::vector<Line> equal{{1,false,1,1000,1},{2,true,1,1000,1}};
    auto s=check(equal,0,100,Error::none); assert(s.net==0 && s.playerAfter==0 && s.vendorAfter==100);
    check(equal,0,0,Error::none);
    equal[1].price=1100;
    check(equal,99,100,Error::playerGold);
    s=check(equal,100,100,Error::none); assert(s.playerAfter==0 && s.vendorAfter==200);
    equal[1].price=800;
    check(equal,0,199,Error::vendorGold);
    s=check(equal,0,200,Error::none); assert(s.playerAfter==200 && s.vendorAfter==0);
    for (int buys=0; buys<31; ++buys) for(int sells=0; sells<31; ++sells) for(int purse: {0,1,10,100}) {
        std::vector<Line> lines{{1,true,1,buys,1},{2,false,1,sells,1}};
        const auto error=buys-sells>purse ? Error::playerGold : sells-buys>purse ? Error::vendorGold : Error::none;
        const auto r=check(lines,purse,purse,error);
        if(r) assert(r.playerAfter+r.vendorAfter==2*purse);
    }
    check(std::vector<Line>{},0,0,Error::empty);
    check(std::vector<Line>{{1,true,2,10,1}},100,100,Error::changed);
    check(std::vector<Line>{{1,true,0,10,1}},100,100,Error::invalid);
    check(std::vector<Line>{{1,true,1,-1,1}},100,100,Error::invalid);
    check(std::vector<Line>{{1,true,1,10,1},{1,true,1,10,1}},100,100,Error::invalid);
    check(std::vector<Line>{{1,true,2,2147483647,2}},100,100,Error::overflow);
    check(std::vector<Line>{{1,true,1,1,1}},1,limit,Error::overflow);
    check(std::vector<Line>{{1,false,1,1,1}},limit,1,Error::overflow);
    check(std::vector<Line>{{1,false,100,0,100}},0,0,Error::none);
    auto many=std::vector<Line>(65); check(many,0,0,Error::tooMany);
    for(int fail=-1; fail<4; ++fail) {
        Mock mock; mock.failAt=fail;
        assert(transact(mock,4)==(fail==-1));
        if(fail>=0) { assert((mock.player==std::array<int,4>{1,0,1,0})); assert(mock.paid==0); }
        else { assert((mock.player==std::array<int,4>{0,1,0,1})); assert(mock.paid==1); }
        ++cases;
    }
    Mock paymentFail; paymentFail.paymentOK=false; assert(!transact(paymentFail,4));
    assert((paymentFail.player==std::array<int,4>{1,0,1,0})); assert((paymentFail.undone==std::vector<std::size_t>{3,2,1,0}));
    Mock stale; stale.valid=false; assert(!transact(stale,4)); assert(stale.paid==0);
    std::cout << cases+2 << " pricing, conservation, inventory-failure and rollback scenarios passed\n";
}
