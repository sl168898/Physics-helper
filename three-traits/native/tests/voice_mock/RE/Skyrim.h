#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include <set>
#include <vector>
namespace RE {
    struct SpellItem {};
    struct BGSPerk {};
    struct EffectSetting {};
    struct MagicTarget {
        std::set<EffectSetting*> effects;
        std::map<SpellItem*, std::vector<EffectSetting*>> sources;
        std::vector<SpellItem*> dispelled;
        bool HasMagicEffect(EffectSetting* effect) { return effects.contains(effect); }
        void DispelEffect(SpellItem* spell, int handle) {
            assert(handle == 7); dispelled.push_back(spell);
            for (auto effect : sources[spell]) effects.erase(effect);
        }
    };
    struct PlayerCharacter {
        static inline PlayerCharacter* instance{};
        MagicTarget target;
        std::set<SpellItem*> spells;
        std::set<BGSPerk*> perks;
        unsigned removals = 0;
        unsigned perkPoints = 5;
        unsigned knownShouts = 4;
        static auto GetSingleton() { return instance; }
        auto GetMagicTarget() { return &target; }
        int GetHandle() { return 7; }
        bool HasSpell(SpellItem* spell) { return spells.contains(spell); }
        bool HasPerk(BGSPerk* perk) { return perks.contains(perk); }
        bool RemoveSpell(SpellItem* spell) { ++removals; return spells.erase(spell) != 0; }
        void RemovePerk(BGSPerk* perk) { ++removals; perks.erase(perk); }
    };
    struct TESDataHandler {
        std::map<std::uint32_t, void*> forms;
        template<class T> T* LookupForm(std::uint32_t id, const char*) {
            const auto it = forms.find(id); return it == forms.end() ? nullptr : static_cast<T*>(it->second);
        }
    };
}
