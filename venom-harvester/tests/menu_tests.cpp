#include "MenuGate.h"
#include <cassert>
#include <iostream>
using namespace harvest;

int main() {
    MenuGate menu;
    auto first = menu.begin();
    assert(first && !menu.begin());
    const MenuRequest remember{1, first, false};
    assert(menu.resolve(remember, 1, 0, true) == true);
    assert(!menu.resolve(remember, 1, 0, true)); // duplicate UI delivery

    auto second = menu.begin();
    assert(second && second != first);
    const MenuRequest cancel{1, second, true};
    assert(!menu.resolve(remember, 1, 0, true)); // must not unlock newer menu
    assert(!menu.begin());
    assert(menu.resolve(cancel, 1, 0, true) == false);

    for (unsigned button : {1u, 2u, 255u, ~0u}) {
        auto ticket = menu.begin();
        assert(ticket);
        assert(!menu.resolve({1, ticket, false}, 1, button, true));
    }
    auto old = menu.begin();
    menu.reset(); // load a different save while a callback is queued
    auto current = menu.begin();
    assert(current && old != current);
    assert(!menu.resolve({1, old, false}, 2, 0, true));
    assert(!menu.begin());
    assert(menu.resolve({2, current, false}, 2, 0, true) == true);

    auto removed = menu.begin();
    assert(!menu.resolve({2, removed, false}, 2, 0, false)); // trait removed
    auto failedFactory = menu.begin();
    assert(failedFactory && menu.finish(failedFactory));
    assert(!menu.finish(failedFactory));
    assert(!menu.finish(0));
    assert(menu.begin());
    std::cout << "Satchel menu: remember, cancel, close, duplicate, stale save, trait removal and failed opening passed\n";
}
