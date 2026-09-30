#include "VoiceAuthorityRuntime.h"
#include <iostream>

struct Fixture {
    RE::TESDataHandler data;
    RE::PlayerCharacter player;
    RE::SpellItem oldAbility, otherAbility;
    RE::BGSPerk oldPerk, newPerk, otherPerk;
    RE::EffectSetting oldEffect, oldCooldown, otherEffect;
    traits::VoiceAuthorityRuntime voice;
    Fixture() {
        RE::PlayerCharacter::instance = &player;
        SKSE::taskInterface.tasks.clear();
        data.forms = {{0x800,&oldAbility},{0x802,&oldPerk},{0x807,&newPerk},{0x801,&oldEffect},{0x806,&oldCooldown}};
        player.target.sources[&oldAbility] = {&oldEffect,&oldCooldown};
        player.target.sources[&otherAbility] = {&otherEffect};
        player.spells = {&oldAbility,&otherAbility};
        player.perks = {&oldPerk,&newPerk,&otherPerk};
        player.target.effects = {&oldEffect,&oldCooldown,&otherEffect};
    }
    void init() { voice.init(&data,"Biggie Traits - Combined.esp"); }
    void load() { voice.loaded(true); SKSE::taskInterface.run(); }
    void verify() {
        assert(!player.HasSpell(&oldAbility) && !player.HasPerk(&oldPerk));
        assert(!player.target.HasMagicEffect(&oldEffect) && !player.target.HasMagicEffect(&oldCooldown));
        assert(player.HasSpell(&otherAbility) && player.HasPerk(&otherPerk));
        assert(player.target.HasMagicEffect(&otherEffect));
        assert(player.perkPoints == 5 && player.knownShouts == 4);
        for (const auto spell : player.target.dispelled) assert(spell == &oldAbility);
    }
};

int main() {
    { Fixture f; f.init(); f.load(); f.verify(); assert(f.player.HasPerk(&f.newPerk));
      const auto n=f.player.removals; f.load(); f.verify(); assert(f.player.removals==n); }
    { Fixture f; f.player.perks.erase(&f.newPerk); f.init(); f.load(); f.verify(); assert(!f.player.HasPerk(&f.newPerk)); }
    { Fixture f; f.player.spells.erase(&f.oldAbility); f.init(); f.load(); f.verify(); }
    { Fixture f; f.player.spells.erase(&f.oldAbility); f.player.perks.erase(&f.oldPerk); f.init(); f.load(); f.verify(); }
    { Fixture f; f.init(); f.voice.loaded(true); f.voice.cancel(); SKSE::taskInterface.run();
      assert(f.player.removals==0 && f.player.target.dispelled.empty()); }
    { Fixture f; f.init(); f.voice.loaded(true); f.voice.loaded(false); SKSE::taskInterface.run();
      assert(f.player.removals==0 && f.player.target.dispelled.empty()); }
    { Fixture f; f.init(); f.voice.loaded(true); f.voice.loaded(true); SKSE::taskInterface.run();
      f.verify(); assert(f.player.target.dispelled.size()==1); }
    { Fixture f; f.data.forms.erase(0x807); f.init(); f.load(); assert(f.player.removals==0); }
    { Fixture f; f.voice.init(nullptr,"missing"); f.load(); assert(f.player.removals==0); }
    { Fixture f; f.init(); RE::PlayerCharacter::instance=nullptr; f.load(); assert(f.player.removals==0); }
    std::cout << "PASS: production Voice of Authority upgrade retires only legacy state, preserves bought perks and other effects, grants no shouts/points/perks, and cancels stale load tasks\n";
}
