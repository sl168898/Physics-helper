#pragma once
#include "GreenThumb.h"
#include <functional>
#include <Windows.h>

namespace traits {
class GreenThumbRuntime {
    using Point = RE::BGSEntryPoint::ENTRY_POINT;
    using HarvestFn = void (*)(Point, RE::Actor*, RE::TESForm*, float*);
    RE::SpellItem* trait{};
    std::function<bool()> inSession;
    bool installed{};
    std::uint32_t diagnostics{};
    static inline GreenThumbRuntime* self{};
    static inline void* original{};

    static void harvested(Point point, RE::Actor* owner, RE::TESForm* ingredient, float* value) {
        // Native ABI: perk owner, ingredient condition target, float result.
        // Let all original perks resolve first, including Set Value perks.
        reinterpret_cast<HarvestFn>(original)(point, owner, ingredient, value);
        auto s = self;
        if (!s || !s->installed || !value || !s->inSession || !s->inSession()) return;
        const auto p = RE::PlayerCharacter::GetSingleton();
        if (!p || owner != p || !s->trait || !p->HasSpell(s->trait) ||
            !ingredient || !ingredient->As<RE::IngredientItem>()) return;
        const float before = *value;
        *value = greenThumb::harvestYield(before, true, true, true);
        if (s->diagnostics++ < 40)
            SKSE::log::info("[GreenThumb] harvested ingredient {:08X}: {} -> {} after other perks",
                ingredient->GetFormID(), before, *value);
    }
public:
    void init(RE::TESDataHandler* data, const char* file, std::function<bool()> live) {
        trait = data ? data->LookupForm<RE::SpellItem>(0xF70, file) : nullptr;
        if (!trait) { SKSE::log::error("Green Thumb: trait ability missing"); return; }
        inSession = std::move(live); self = this;
        const auto target = REL::Relocation<std::uintptr_t>{REL::RelocationID(23073, 23526)}.address();
        const auto bytes = greenThumb::harvestGate(reinterpret_cast<std::uintptr_t>(&harvested),
            reinterpret_cast<std::uintptr_t>(&original));
        auto gate = SKSE::GetTrampoline().allocate(bytes.size());
        std::memcpy(gate, bytes.data(), bytes.size());
        FlushInstructionCache(GetCurrentProcess(), gate, bytes.size());
        if (auto status = MH_CreateHook(reinterpret_cast<void*>(target), gate, &original); status != MH_OK) {
            SKSE::log::error("Green Thumb: harvest hook creation failed {}", int(status)); return;
        }
        if (auto status = MH_EnableHook(reinterpret_cast<void*>(target)); status != MH_OK) {
            SKSE::log::error("Green Thumb: harvest hook enable failed {}", int(status)); return;
        }
        installed = true;
        SKSE::log::info("Green Thumb ready: ingredient harvest x2 after existing perks; shared by Satchel refunds");
    }
};
}
