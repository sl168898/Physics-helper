#include "Rules.h"
#include <cassert>
int main() {
 using traits::Combat;
 Combat g;g.block();g.tick(2);g.block();g.tick(3);g.block();assert(g.guardReady);
 g.beginSwing(true,false,true,false,true,false);assert(g.damage(true,false,true)==5);assert(!g.guardReady);
 // All contacts from the same bash share its bonus; the next bash does not.
 assert(g.damage(true,false,true)==5);g.endSwing();g.beginSwing(true,false,true,false,true,false);assert(g.damage(true,false,true)==1);
 Combat r;r.block();r.tick(2);r.block();r.tick(3.01f);r.block();assert(!r.guardReady);r.tick(1);r.block();assert(r.guardReady);
 Combat e;e.shout();e.tick(5);e.beginSwing(false,true,true,false,false,true);assert(e.damage(false,true,true)==1.5f);e.endSwing();e.beginSwing(false,true,true,true,false,true);assert(e.damage(false,true,true)==1);
 e.shout();e.beginSwing(false,true,true,true,false,true);assert(e.damage(false,true,true)==2);assert(e.damage(false,false,true)==1);assert(e.damage(true,true,true)==1);
 Combat expired;expired.shout();expired.tick(5.001f);expired.beginSwing(false,true,true,true,false,true);assert(expired.damage(false,true,true)==1);
 Combat miss;miss.shout();miss.beginSwing(false,true,true,false,false,true);miss.endSwing();miss.beginSwing(false,true,true,false,false,true);assert(miss.damage(false,true,true)==1);
 Combat non;non.shout();non.beginSwing(false,false,true,false,false,true);non.endSwing();non.beginSwing(false,true,true,true,false,true);assert(non.damage(false,true,true)==2);
 non.clearEcho();assert(non.damage(false,true,true)==1);
}
