#include "VisualRules.h"
#include <cassert>
int main() {
    using namespace runes::visuals;
    // Temporary visuals supersede the permanent appearance, then it returns.
    assert(choose(2, 1) == 2);
    assert(choose(0, 1) == 1);
    assert(choose(2, 0) == 2);
    assert(choose(0, 0) == 0);
    assert(eligible(true, true, true, true, false, false));
    assert(!eligible(false, true, true, true, false, false));
    assert(!eligible(true, false, true, true, false, false));
    assert(!eligible(true, true, false, true, false, false));
    assert(!eligible(true, true, true, false, false, false));
    assert(!eligible(true, true, true, true, true, false));
    assert(!eligible(true, true, true, true, false, true));
    // Stable models keep continuous effects. Reload/camera changes, a different
    // weapon, replaced rune, or finished native effect require a new attachment.
    assert(reuse(10, 10, 1, 1, true, true));
    assert(!reuse(10, 20, 1, 1, true, true));
    assert(!reuse(10, 10, 1, 2, true, true));
    assert(!reuse(10, 10, 1, 1, false, true));
    assert(!reuse(10, 10, 1, 1, true, false));
    assert(!reuse(0, 0, 1, 1, true, true));
}
