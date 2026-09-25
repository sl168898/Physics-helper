#include "ArcaneDynamo.h"
#include <cassert>
#include <iostream>

bool near(float a, float b) { return std::abs(a-b) < 0.0001f; }
int main() {
    using namespace traits::dynamo;
    assert(near(cost(40,40),15));
    assert(near(cost(40,30),11.25));
    assert(near(cost(40,20),7.5));
    assert(near(cost(40,60),22.5));
    assert(near(cost(40,120),45));
    assert(cost(40,0)==0 && cost(0,0)==0);
    assert(!canPay(100,cost(0,20)));
    assert(!canPay(14.99f,15));
    assert(canPay(15,15) && canPay(0,0));
    assert(!canPay(100,std::numeric_limits<float>::quiet_NaN()));
    assert(!canPay(std::numeric_limits<float>::infinity(),15));
    assert(damageEffect(0,true,false,false,24,-1));
    assert(damageEffect(4,true,false,false,25,-1));
    assert(damageEffect(5,true,false,false,24,26));
    assert(!damageEffect(34,true,true,false,24,-1)); // temporary maximum Health reduction
    assert(!damageEffect(0,true,false,false,41,-1)); // resistance debuff
    assert(!damageEffect(1,true,false,false,24,-1)); // scripted proc
    assert(!damageEffect(21,true,false,true,24,-1)); // paralysis
    assert(!damageEffect(0,false,false,false,24,-1)); // healing
    assert(near(damage(40,true),52));
    assert(near(damage(40*2,true),104)); // Runic Overdrive remains x2, then Dynamo x1.30
    assert(near(damage(40,false),40));
    assert(near(regeneration(3,true),1.95f));
    assert(near(regeneration(6,true),3.9f)); // other regeneration bonuses retained
    assert(regeneration(0,true)==0 && regeneration(-1,true)==-1);
    int context=0;
    { Scope<int> first(context,1); assert(context==1);
      { Scope<int> nested(context,2); assert(context==2); }
      assert(context==1); }
    assert(context==0);
    std::cout << "Dynamo: cost modifiers, payment threshold, damage classification, Overdrive stacking, regeneration and nested isolation passed\n";
}
