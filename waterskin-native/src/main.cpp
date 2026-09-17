#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "RewardRule.h"
#include <atomic>
#include <mutex>

namespace
{
    RE::BGSConstructibleObject* recipe = nullptr;
    RE::AlchemyItem* filled = nullptr;
    RE::AlchemyItem* water = nullptr;
    RE::TESObjectMISC* empty = nullptr;
    RE::BGSKeyword* cooking = nullptr;
    bool ready = false;
    std::atomic_bool awarding = false;

    bool validRecipe()
    {
        if (!recipe || !filled || !water || !empty || !cooking) return false;
        if (recipe->createdItem != water || recipe->data.numConstructed != 3 || recipe->benchKeyword != cooking)
            return false;
        const auto& items = recipe->requiredItems;
        return items.numContainerObjects == 1 && items.containerObjects && items.containerObjects[0] &&
            items.containerObjects[0]->obj == filled && items.containerObjects[0]->count == 1;
    }

    class Events final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
                         public RE::BSTEventSink<RE::TESContainerChangedEvent>,
                         public RE::BSTEventSink<RE::ItemCrafted::Event>
    {
    public:
        static Events& get() { static Events instance; return instance; }
        using Result = RE::BSEventNotifyControl;

        void reset()
        {
            std::lock_guard lock(mutex);
            ++epoch; active = false; queued = false; pending = {};
        }

        Result ProcessEvent(const RE::MenuOpenCloseEvent* e, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!e || e->menuName != RE::CraftingMenu::MENU_NAME) return Result::kContinue;
            std::lock_guard lock(mutex);
            if (e->opening) {
                ++epoch; queued = false; pending = {}; active = true;
                SKSE::log::info("Crafting menu opened; recipe valid={}", ready && validRecipe());
            } else {
                active = false;
                queue();
                SKSE::log::info("Crafting menu closed");
            }
            return Result::kContinue;
        }

        Result ProcessEvent(const RE::TESContainerChangedEvent* e, RE::BSTEventSource<RE::TESContainerChangedEvent>*) override
        {
            if (!e || !ready || awarding.load() || e->itemCount <= 0) return Result::kContinue;
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!player) return Result::kContinue;
            const auto playerID = player->GetFormID();
            const bool removedSkin = e->baseObj == filled->GetFormID() && e->oldContainer == playerID && e->newContainer == 0;
            const bool addedWater = e->baseObj == water->GetFormID() && e->newContainer == playerID && e->oldContainer == 0;
            const bool addedEmpty = e->baseObj == empty->GetFormID() && e->newContainer == playerID && e->oldContainer == 0;
            if (!removedSkin && !addedWater && !addedEmpty) return Result::kContinue;
            std::lock_guard lock(mutex);
            if (!active) return Result::kContinue;
            if (removedSkin) pending.skins += e->itemCount;
            if (addedWater) pending.waters += e->itemCount;
            if (addedEmpty) pending.empties += e->itemCount;
            SKSE::log::info("Crafting inventory event: form={:08X}, count={}, removedSkin={}, addedWater={}, addedEmpty={}",
                e->baseObj, e->itemCount, removedSkin, addedWater, addedEmpty);
            queue();
            return Result::kContinue;
        }

        Result ProcessEvent(const RE::ItemCrafted::Event* e, RE::BSTEventSource<RE::ItemCrafted::Event>*) override
        {
            if (!e || !e->item || !ready) return Result::kContinue;
            std::lock_guard lock(mutex);
            if (!active) return Result::kContinue;
            if (e->item == water) ++pending.waterCrafts;
            else ++pending.otherCrafts;
            SKSE::log::info("ItemCrafted event: form={:08X}, bottledWater={}", e->item->GetFormID(), e->item == water);
            queue();
            return Result::kContinue;
        }

    private:
        struct Transaction {
            std::int64_t skins = 0, waters = 0, empties = 0;
            std::uint32_t waterCrafts = 0, otherCrafts = 0;
        } pending;
        std::mutex mutex;
        std::uint64_t epoch = 0;
        bool active = false, queued = false;

        // Called with mutex held. SKSE tasks run after the current game operation.
        // Ingredient/output/craft notifications can arrive in either order.
        void queue()
        {
            if (queued) return;
            queued = true;
            const auto generation = epoch;
            SKSE::GetTaskInterface()->AddTask([this, generation] { flush(generation); });
        }

        void flush(std::uint64_t generation)
        {
            Transaction transaction;
            {
                std::lock_guard lock(mutex);
                if (generation != epoch) return;
                queued = false;
                if (active && pending.waterCrafts == 0 && pending.otherCrafts == 0) return;
                transaction = pending;
                pending = {}; // Claim once, before AddObjectToContainer emits more events.
            }
            const auto amount = waterskin::transactionReward(ready && validRecipe(), transaction.skins,
                transaction.waters, transaction.empties, transaction.waterCrafts, transaction.otherCrafts);
            if (auto player = RE::PlayerCharacter::GetSingleton(); player && amount > 0) {
                awarding.store(true);
                player->AddObjectToContainer(empty, nullptr, amount, nullptr);
                awarding.store(false);
                SKSE::log::info("Returned {} empty waterskin(s); consumed={}, crafted water={}", amount, transaction.skins, transaction.waters);
            } else if (transaction.skins || transaction.waters || transaction.waterCrafts) {
                SKSE::log::warn("No matched transaction: skins={}, waters={}, existing empties={}, water craft signals={}, other craft signals={}",
                    transaction.skins, transaction.waters, transaction.empties, transaction.waterCrafts, transaction.otherCrafts);
            }
        }
    };

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            auto data = RE::TESDataHandler::GetSingleton();
            recipe = data->LookupForm<RE::BGSConstructibleObject>(0x800, "Personal Tweaks.esp");
            filled = data->LookupForm<RE::AlchemyItem>(0x801, "Waterskin.esp");
            empty = data->LookupForm<RE::TESObjectMISC>(0xD61, "Waterskin.esp");
            water = data->LookupForm<RE::AlchemyItem>(0x3DFE8, "Requiem.esp");
            cooking = data->LookupForm<RE::BGSKeyword>(0xA5CB3, "Skyrim.esm");
            ready = validRecipe();
            if (ready) SKSE::log::info("Ready: original waterskin bottling recipe resolved and validated");
            else SKSE::log::error("Disabled: expected recipe/items absent or overridden. Disable the old batch-item patch.");
            auto& events = Events::get();
            RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESContainerChangedEvent>(&events);
            RE::ItemCrafted::GetEventSource()->AddEventSink(&events);
            SKSE::log::info("Registered crafting menu, inventory and ItemCrafted listeners; no confirmation hook");
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
                   message->type == SKSE::MessagingInterface::kNewGame ||
                   message->type == SKSE::MessagingInterface::kPostLoadGame) {
            Events::get().reset();
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({ 1, 1, 0, 0 });
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
    SKSE::log::info("WaterskinBottleReturn 1.1.0; target Skyrim 1.6.1170; no additional forms or scripts");
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
