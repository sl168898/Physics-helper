#include "IronLungsImpact.h"
#include <cassert>

int main() {
    using namespace traits;
    auto first = std::make_shared<IronLungsCast>();
    auto second = std::make_shared<IronLungsCast>();
    first->serial = 1; first->quote = quoteIronLungs(420, 420, 1);
    second->serial = 2; second->quote = quoteIronLungs(300, 420, 1);
    IronLungsImpact current;
    const std::uint32_t player = 0x14, spell = 0x13F3A, target = 42;
    // Use the production context/claim policy: later projectiles may arrive
    // first, and every effect of one shout must share its one target claim.
    auto apply = [&](std::uint32_t source, std::uint32_t who, std::uint32_t to,
                     bool accepted = true, bool bonus = false) {
        if (!current.matches(source, who, to, accepted, bonus) || !current.cast->claimTarget(to)) return 0.f;
        return current.cast->quote.bonus;
    };
    assert(apply(spell, player, target) == 0);
    {
        IronLungsImpactScope hit(current, {second, spell, player, target});
        assert(apply(spell, player, target, false) == 0); // rejected effect does not consume claim
        assert(apply(spell, player, target, true, true) == 0); // bonus recursion
        assert(apply(spell, 0x100, target) == 0); // other caster
        assert(apply(spell + 1, player, target) == 0); // another spell/word
        assert(apply(spell, player, target + 1) == 0); // another target
        {
            IronLungsImpactScope unrelated(current, {}); // untracked/Skald nested impact
            assert(apply(spell, player, target) == 0);
        }
        assert(current.cast == second);
        assert(apply(spell, player, target) == 75);
        assert(apply(spell, player, target) == 0); // multiple native effects
        {
            IronLungsImpactScope olderProjectile(current, {first, spell, player, target});
            assert(apply(spell, player, target) == 105); // original, not latest snapshot
        }
        assert(current.cast == second && apply(spell, player, target) == 0);
    }
    assert(!current.cast && apply(spell, player, target) == 0);
    {
        IronLungsImpactScope sameShoutAnotherProjectile(current, {first, spell, player, target});
        assert(apply(spell, player, target) == 0); // once per cast, not once per projectile
    }
    {
        IronLungsImpactScope anotherEnemy(current, {first, spell, player, target + 1});
        assert(apply(spell, player, target + 1) == 105); // cone hits multiple actors
    }
    assert(first->quote.bonus == 105 && second->quote.bonus == 75);
    IronLungsImpact incomplete{first, spell, player, 0};
    assert(!incomplete.matches(spell, player, 0, true, false));
    // These tests prove transaction isolation, not the engine hook's runtime
    // reachability or final health loss through native resistance/absorption.
}
