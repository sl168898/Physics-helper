#include "Policy.h"
#include <array>
#include <cassert>
int main() {
 using E=boozy::EffectView;
 std::array<E,0> none{};
 assert(!boozy::blocked(true,false,none));
 assert(boozy::blocked(true,true,none)); // same-frame second drink before first is listed
 assert(!boozy::blocked(false,true,none)); // ordinary potions pass through
 std::array<E,4> saved{{{true,false,71,300},{true,false,189,750},{true,false,21,450},{true,false,10,450}}};
 assert(boozy::oldest(saved)==1); // rum is oldest; two separate meads are duplicates
 assert(boozy::blocked(true,false,saved));
 std::array<E,4> lifecycle{{{true,true,40,750},{true,false,750,750},{false,false,20,900},{true,false,100,750}}};
 assert(boozy::oldest(lifecycle)==3); // ignores dispelled, expired and unrelated effects
 lifecycle[3].dispelled=true;
 assert(!boozy::blocked(true,false,lifecycle)); // next drink allowed after expiry/cleanup
 std::array<E,2> equal{{{true,false,0,750},{true,false,0,300}}};
 assert(boozy::oldest(equal)==0); // deterministic simultaneous effects
 std::array<E,1> deferred{{{true,false,0,750}}};
 assert(boozy::blocked(true,false,deferred)); // initial inactive/native pending effect reserves slot
}
