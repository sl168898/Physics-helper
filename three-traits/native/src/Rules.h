#pragma once
#include <algorithm>
#include <cmath>
#include <deque>

namespace traits {
struct Combat {
    double now = 0, echoUntil = -1;
    std::deque<double> blocks;
    bool guardReady = false, swing = false, bash = false, power = false;
    float swingMultiplier = 1;
    void tick(float dt) { if (std::isfinite(dt) && dt > 0) now += dt; }
    void block() {
        if (guardReady) return;
        while (!blocks.empty() && now - blocks.front() > 5.) blocks.pop_front();
        blocks.push_back(now);
        if (blocks.size() >= 3) { guardReady = true; blocks.clear(); }
    }
    void shout() { echoUntil = now + 5.; }
    void clearGuard() { blocks.clear(); guardReady = false; if (bash) swingMultiplier = 1; }
    void clearEcho() { echoUntil = -1; if (!bash) swingMultiplier = 1; }
    void endSwing() { swing = bash = power = false; swingMultiplier = 1; }
    void beginSwing(bool isBash, bool isPower, bool melee, bool twoHanded, bool guardSelected, bool echoSelected) {
        endSwing(); swing = true; bash = isBash; power = isPower;
        if (isBash && guardSelected && guardReady) { swingMultiplier = 5; guardReady = false; blocks.clear(); }
        else if (!isBash && isPower && melee && echoSelected && now <= echoUntil) {
            swingMultiplier = twoHanded ? 2.f : 1.5f; echoUntil = -1;
        }
    }
    float damage(bool isBash, bool isPower, bool melee) const {
        return swing && isBash == bash && isPower == power && (isBash || melee) ? swingMultiplier : 1.f;
    }
};
}
