#include "LabVisit.h"
#include <cassert>
int main() {
    traits::LabVisit v;
    assert(!v.finish());
    v.enter(false); assert(!v.finish()); // chairs, smithing and enchanting
    v.enter(true); assert(v.finish()); assert(!v.finish()); // furniture-only route
    v.confirmAlchemyMenu(); assert(v.finish()); assert(!v.finish()); // menu-only route
    v.enter(true); v.confirmAlchemyMenu(); assert(v.finish()); assert(!v.finish());
    v.enter(true); v.cancel(); assert(!v.finish()); // load/trait removal
    v.enter(true); v.enter(false); assert(!v.finish()); // unrelated furniture replaces stale visit
    v.enter(true); assert(v.finish()); v.enter(true); assert(v.finish()); // new visit refreshes
}
