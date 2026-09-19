#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <memory>
#include "CraftingRules.h"

namespace {
constexpr auto pluginFile = "Star Runestone Recipes.esp";
struct Recipe {
    RE::TESSoulGem* star{};
    RE::TESObjectMISC* output{};
    RE::BGSConstructibleObject* form{};
    RE::TESGlobal* gate{};
};
std::array<Recipe, 4> recipes;
std::atomic_bool ready{false}, session{false};
std::atomic_uint64_t epoch{0};
thread_local bool insideAdd{};
using Remove = RE::ObjectRefHandle* (*)(RE::PlayerCharacter*, RE::ObjectRefHandle*, RE::TESBoundObject*,
    std::int32_t, RE::ITEM_REMOVE_REASON, RE::ExtraDataList*, RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);
using Add = void (*)(RE::PlayerCharacter*, RE::TESBoundObject*, RE::ExtraDataList*, std::int32_t, RE::TESObjectREFR*);
REL::Relocation<Remove> originalRemove;
REL::Relocation<Add> originalAdd;
RE::SCRIPT_FUNCTION::Condition_t* originalGlobalCondition{};

RE::ExtraDataList* findSoul(RE::PlayerCharacter* player, unsigned index) {
    auto form = recipes[index].star;
    if (!player || !form || form->GetContainedSoul() != RE::SOUL_LEVEL::kNone) return nullptr;
    auto items = player->GetInventory([form](RE::TESBoundObject& item) { return &item == form; });
    auto it = items.find(form);
    if (it == items.end() || it->second.first <= 0) return nullptr;
    const auto& entry = it->second.second;
    if (!entry || !entry->extraLists) return nullptr;
    for (auto extra : *entry->extraLists) {
        if (!extra) continue;
        const auto soul = extra->GetByType<RE::ExtraSoul>();
        if (soul && stars::eligible(index, form->GetFormID(),
            static_cast<unsigned>(soul->GetContainedSoul()), extra->GetCount())) return extra;
    }
    return nullptr;
}

// Four private, zero-valued globals are only condition markers. Evaluate the
// ACTUAL inventory instance when Skyrim checks the recipe. No polling, stale
// cached soul sizes, or save-persisted availability flags. Without this DLL the
// globals stay zero and all new recipes fail closed.
bool globalCondition(RE::TESObjectREFR* reference, void* param1, void* param2, double& result) {
    for (unsigned i = 0; i < recipes.size(); ++i) {
        if (recipes[i].gate && param1 == recipes[i].gate) {
            result = ready && session && !RE::TaskQueueInterface::ShouldUseTaskQueue() &&
                findSoul(RE::PlayerCharacter::GetSingleton(), i) ? 1.0 : 0.0;
            return true;
        }
    }
    return originalGlobalCondition(reference, param1, param2, result);
}

int selectedRecipe() {
    if (!ready || RE::TaskQueueInterface::ShouldUseTaskQueue()) return -1;
    auto ui = RE::UI::GetSingleton();
    auto menu = ui ? ui->GetMenu<RE::CraftingMenu>() : nullptr;
    auto sub = menu ? skyrim_cast<RE::CraftingSubMenus::ConstructibleObjectMenu*>(menu->GetCraftingSubMenu()) : nullptr;
    if (!sub || sub->currentCobjIdx >= sub->crafts.size()) return -1;
    auto selected = sub->crafts[sub->currentCobjIdx].constructibleObject;
    for (unsigned i = 0; i < recipes.size(); ++i) if (selected == recipes[i].form) return static_cast<int>(i);
    return -1;
}

struct Inventory {
    RE::PlayerCharacter* player;
    unsigned index;
    RE::ExtraDataList* instance;
    RE::ExtraDataList* outputExtra;
    RE::TESObjectREFR* source;
    bool eligible(unsigned requested) const {
        return index == requested && instance && findSoul(player, index) == instance;
    }
    int outputCount() const {
        auto output = recipes[index].output;
        auto counts = player->GetInventoryCounts([output](RE::TESBoundObject& item) { return &item == output; });
        auto it = counts.find(output);
        return it == counts.end() ? 0 : it->second;
    }
    bool setSoul(unsigned expected, unsigned value) const {
        // Reacquire the live list. Never dereference a saved pointer after an
        // external inventory hook unless it is still in this player's entry.
        auto changes = player->GetInventoryChanges();
        if (!changes || !changes->entryList) return false;
        for (auto entry : *changes->entryList) {
            if (!entry || entry->object != recipes[index].star || !entry->extraLists) continue;
            for (auto extra : *entry->extraLists) {
                if (extra != instance) continue;
                if (extra->GetCount() != 1) return false;
                auto soul = extra->GetByType<RE::ExtraSoul>();
                if (!soul || static_cast<unsigned>(soul->GetContainedSoul()) != expected) return false;
                soul->soul = static_cast<RE::SOUL_LEVEL>(value);
                changes->changed = true;
                player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
                return true;
            }
        }
        return false;
    }
    void give(int count) const { originalAdd(player, recipes[index].output, outputExtra, count, source); }
};

RE::ObjectRefHandle* removeHook(RE::PlayerCharacter* player, RE::ObjectRefHandle* returnBuffer,
    RE::TESBoundObject* item, std::int32_t count, RE::ITEM_REMOVE_REASON reason,
    RE::ExtraDataList* extra, RE::TESObjectREFR* destination, const RE::NiPoint3* drop, const RE::NiPoint3* rotation) {
    const auto index = selectedRecipe();
    if (index >= 0 && player == RE::PlayerCharacter::GetSingleton() &&
        item == recipes[index].star && !destination) {
        // Ingredient removal is intentionally suppressed. The matched output
        // add below consumes only ExtraSoul, independent of add/remove order.
        // MSVC x64 passes the non-trivial handle's return buffer after `this`.
        std::construct_at(returnBuffer);
        return returnBuffer;
    }
    return originalRemove(player, returnBuffer, item, count, reason, extra, destination, drop, rotation);
}

void addHook(RE::PlayerCharacter* player, RE::TESBoundObject* item, RE::ExtraDataList* extra,
    std::int32_t count, RE::TESObjectREFR* source) {
    const auto index = selectedRecipe();
    if (index < 0 || player != RE::PlayerCharacter::GetSingleton() || item != recipes[index].output || source) {
        originalAdd(player, item, extra, count, source); return;
    }
    // Reject reentrant/invalid attempts instead of manufacturing free stones.
    if (insideAdd || !session || count != stars::recipes[index].count || extra) return;
    struct Guard { Guard() { insideAdd = true; } ~Guard() { insideAdd = false; } } guard;
    Inventory inventory{player, static_cast<unsigned>(index), findSoul(player, index), extra, source};
    const auto result = stars::craft(inventory, index);
    if (result == stars::Result::crafted) {
        SKSE::log::info("Crafted {} runestones {:08X}; emptied and retained Star {:08X}; soul size {}",
            count, item->GetFormID(), recipes[index].star->GetFormID(), stars::recipes[index].soul);
        RE::DebugNotification("Runestones crafted. The Star is empty and reusable.");
    } else if (result == stars::Result::deliveryFailed) {
        RE::DebugNotification("Runestones could not be added. The soul remains in the Star.");
    } else if (result == stars::Result::rollbackFailed) {
        RE::DebugNotification("Crafting interrupted by an inventory change. Reload your previous save.");
        SKSE::log::critical("Runestone delivery and soul restoration both failed after an external inventory change");
    } else {
        RE::DebugNotification("The Star no longer contains the required soul.");
    }
    const auto ticket = epoch.load();
    SKSE::GetTaskInterface()->AddTask([ticket, index] {
        if (!stars::currentTask(session.load(), ticket, epoch.load())) return;
        if (auto current = RE::PlayerCharacter::GetSingleton())
            RE::SendUIMessage::SendInventoryUpdateMessage(current, recipes[index].star);
    });
}

bool validRecipe(unsigned i) {
    const auto& recipe = recipes[i];
    auto form = recipe.form;
    if (!form || !recipe.star || !recipe.output || !recipe.gate || recipe.gate->value != 0.f ||
        recipe.star->GetContainedSoul() != RE::SOUL_LEVEL::kNone ||
        form->createdItem != recipe.output || form->data.numConstructed != stars::recipes[i].count ||
        form->requiredItems.numContainerObjects != 1 || !form->requiredItems.containerObjects) return false;
    auto ingredient = form->requiredItems.containerObjects[0];
    auto condition = form->conditions.head;
    return ingredient && ingredient->obj == recipe.star && ingredient->count == 1 &&
        condition && !condition->next &&
        condition->data.functionData.function == RE::FUNCTION_DATA::FunctionID::kGetGlobalValue &&
        condition->data.functionData.params[0] == recipe.gate &&
        condition->data.comparisonValue.f == 1.f && !condition->data.flags.global &&
        condition->data.flags.opCode == RE::CONDITION_ITEM_DATA::OpCode::kEqualTo;
}

void initialize() {
    auto data = RE::TESDataHandler::GetSingleton();
    if (!data) return;
    for (unsigned i = 0; i < recipes.size(); ++i) {
        recipes[i] = {RE::TESForm::LookupByID<RE::TESSoulGem>(stars::recipes[i].starID),
            data->LookupForm<RE::TESObjectMISC>(stars::recipes[i].outputID, "RunemasterMagic.esl"),
            data->LookupForm<RE::BGSConstructibleObject>(0x800 + i, pluginFile),
            data->LookupForm<RE::TESGlobal>(0x810 + i, pluginFile)};
        if (!validRecipe(i)) { SKSE::log::error("Missing or modified Star recipe {}. All new recipes remain disabled.", i); return; }
    }
    auto command = RE::SCRIPT_FUNCTION::LocateScriptCommand("GetGlobalValue");
    if (!command || !command->conditionFunction) { SKSE::log::error("GetGlobalValue condition is unavailable"); return; }
    originalGlobalCondition = command->conditionFunction;
    REL::safe_write(reinterpret_cast<std::uintptr_t>(&command->conditionFunction),
        reinterpret_cast<std::uintptr_t>(&globalCondition));
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
    originalRemove = table.write_vfunc(0x56, removeHook);
    originalAdd = table.write_vfunc(0x5A, addHook);
    ready = true;
    SKSE::log::info("Ready: four forge recipes; live soul-size checks; original Star instances preserved");
}

void message(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded: initialize(); break;
    case SKSE::MessagingInterface::kPreLoadGame: session = false; ++epoch; break;
    case SKSE::MessagingInterface::kNewGame: ++epoch; session = true; break;
    case SKSE::MessagingInterface::kPostLoadGame: ++epoch; session = message->data != nullptr; break;
    default: break;
    }
}
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({1,0,0,0}); data.PluginName("StarRunestoneRecipes");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}}); return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory(); if (!path) return false;
    *path /= "StarRunestoneRecipes.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("Star Runestone Recipes 1.0.0; Skyrim Steam 1.6.1170; Runemaster Magic 1.5");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
