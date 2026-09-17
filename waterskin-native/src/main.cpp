#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "RewardRule.h"

namespace
{
    using Menu = RE::CraftingSubMenus::ConstructibleObjectMenu;
    using Callback = Menu::CreationConfirmCallback;
    using Message = RE::IMessageBoxCallback::Message;
    using Run = void (*)(Callback*, Message);

    REL::Relocation<Run> original;
    RE::BGSConstructibleObject* recipe = nullptr;
    RE::AlchemyItem* filled = nullptr;
    RE::AlchemyItem* water = nullptr;
    RE::TESObjectMISC* empty = nullptr;
    RE::BGSKeyword* cooking = nullptr;
    bool ready = false;
    thread_local bool inside = false;

    bool validRecipe()
    {
        if (!recipe || !filled || !water || !empty || !cooking) return false;
        if (recipe->createdItem != water || recipe->data.numConstructed != 3 || recipe->benchKeyword != cooking)
            return false;
        const auto& items = recipe->requiredItems;
        return items.numContainerObjects == 1 && items.containerObjects && items.containerObjects[0] &&
            items.containerObjects[0]->obj == filled && items.containerObjects[0]->count == 1;
    }

    waterskin::Counts inventory(RE::PlayerCharacter* player)
    {
        waterskin::Counts result;
        const auto counts = player->GetInventoryCounts([](RE::TESBoundObject& item) {
            return &item == filled || &item == water || &item == empty;
        });
        for (const auto& [item, count] : counts) {
            if (item == filled) result.filled = count;
            else if (item == water) result.water = count;
            else if (item == empty) result.empty = count;
        }
        return result;
    }

    void hookedRun(Callback* self, Message message)
    {
        // Preserve the original confirmation behavior, including other hooks.
        if (inside || !ready || !self || !self->menu || !validRecipe()) {
            original(self, message);
            return;
        }
        const auto menu = self->menu;
        if (menu->currentCobjIdx >= menu->crafts.size() ||
            menu->crafts[menu->currentCobjIdx].constructibleObject != recipe) {
            original(self, message);
            return;
        }
        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            original(self, message);
            return;
        }
        struct Guard {
            Guard() { inside = true; }
            ~Guard() { inside = false; }
        } guard;
        const auto before = inventory(player);
        original(self, message);
        // The callback may alter/close the menu: do not dereference it again.
        const auto after = inventory(player);
        const auto amount = waterskin::reward(true, before, after);
        if (amount > 0) {
            player->AddObjectToContainer(empty, nullptr, amount, nullptr);
            SKSE::log::info("Returned {} empty waterskin(s) after recipe {:08X}", amount, recipe->GetFormID());
        } else {
            SKSE::log::info("No reward: filled {} -> {}, water {} -> {}, empty {} -> {}",
                before.filled, after.filled, before.water, after.water, before.empty, after.empty);
        }
    }

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kPostPostLoad) {
            REL::Relocation<std::uintptr_t> table{ RE::VTABLE_CraftingSubMenus__ConstructibleObjectMenu__CreationConfirmCallback[0] };
            original = table.write_vfunc(1, hookedRun);
            SKSE::log::info("Installed crafting confirmation hook");
        } else if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            auto data = RE::TESDataHandler::GetSingleton();
            recipe = data->LookupForm<RE::BGSConstructibleObject>(0x800, "Personal Tweaks.esp");
            filled = data->LookupForm<RE::AlchemyItem>(0x801, "Waterskin.esp");
            empty = data->LookupForm<RE::TESObjectMISC>(0xD61, "Waterskin.esp");
            water = data->LookupForm<RE::AlchemyItem>(0x3DFE8, "Requiem.esp");
            cooking = data->LookupForm<RE::BGSKeyword>(0xA5CB3, "Skyrim.esm");
            ready = validRecipe();
            if (ready) SKSE::log::info("Ready: original waterskin bottling recipe resolved and validated");
            else SKSE::log::error("Disabled: expected recipe/items absent or overridden. Disable the old batch-item patch; expected one filled skin -> three original bottled waters at a cooking station.");
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({ 1, 0, 0, 0 });
    data.PluginName("WaterskinBottleReturn");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true);
    data.UsesStructsPost629(true);
    data.CompatibleVersions({ REL::Version{ 1, 6, 1170, 0 } });
    return data;
}();

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != REL::Version{ 1, 6, 1170, 0 }) return false;
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "WaterskinBottleReturn.log";
    auto logger = std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("WaterskinBottleReturn 1.0.0; target Skyrim 1.6.1170; no additional forms or scripts");
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
