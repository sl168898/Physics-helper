#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <Windows.h>
#include "Core.h"
#include "Coating.h"
#include "ImmersiveAnimation.h"
#include "AmmoIconEffects.h"
#include "PoisonSnapshot.h"
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace
{
    constexpr std::uint32_t saveID = 0x4E415031, recordID = 0x52435031;
    struct Settings
    {
        std::uint32_t key = 66, arrowsPerBottle = 0, maxBatch = 5000;
        bool autoEquip = true, trace = false, craftOnUse = true, immersiveAnimation = true;
        void load()
        {
            constexpr auto path = ".\\Data\\SKSE\\Plugins\\PoisonedAmmoNative.ini";
            key = GetPrivateProfileIntA("General", "CraftKey", 66, path);
            arrowsPerBottle = std::min(GetPrivateProfileIntA("General", "ArrowsPerBottle", 0, path), 10000u);
            maxBatch = std::clamp(GetPrivateProfileIntA("General", "MaxBatchArrows", 5000, path), 1u, 100000u);
            autoEquip = GetPrivateProfileIntA("General", "AutoEquip", 1, path) != 0;
            craftOnUse = GetPrivateProfileIntA("General", "CraftOnPoisonUse", 1, path) != 0;
            immersiveAnimation = GetPrivateProfileIntA("General", "ImmersiveInteractionsBridge", 1, path) != 0;
            trace = GetPrivateProfileIntA("General", "TraceProjectiles", 0, path) != 0;
            if (key > 255) key = 66;
        }
    } settings;
    struct Slot
    {
        RE::TESAmmo* form{};
        RE::AlchemyItem* proxy{};
        RE::TESAmmo* original{};
        RE::AlchemyItem* poison{};
        std::uint32_t iconInfo{}; // Refreshed with the saved recipe, read under stateMutex.
    };
    std::array<Slot, pa::capacity> slots;
    std::unordered_map<RE::FormID, std::size_t> slotIDs;
    pa::Recipes recipes;
    std::recursive_mutex stateMutex;
    RE::TESGlobal* marker{};
    bool formsReady = false, saveFault = false;
    std::atomic_bool session = false, menuPending = false, externalPending = false;
    std::atomic<std::uint64_t> generation = 0;
    // Engine active effects can retain Effect pointers across load/revert callbacks.
    // Retire them for this process instead of freeing memory still in engine use.
    std::vector<RE::Effect*> effectArena;

    void notify(std::string_view message) { RE::DebugNotification(std::string(message).c_str()); }
    std::string displayName(RE::TESForm* form)
    {
        const char* name = form ? form->GetName() : nullptr;
        return name && *name ? std::string(name).substr(0, 110) : "Unnamed";
    }
    std::optional<pa::Key> keyOf(RE::TESForm* form)
    {
        if (!form || form->IsDynamicForm()) return {};
        const auto file = form->GetFile(0);
        if (!file) return {};
        pa::Key result{pa::lower(std::string(file->GetFilename())), pa::localID(form->GetFormID(), file->IsLight())};
        if (!pa::valid(result)) return {};
        return result;
    }
    template<class T> T* resolve(const pa::Key& key)
    {
        auto data = RE::TESDataHandler::GetSingleton();
        if (!data || !pa::valid(key)) return nullptr;
        const auto file = data->LookupModByName(key.file);
        if (!file || (file->IsLight() && key.local > 0xFFF)) return nullptr;
        return data->LookupForm<T>(key.local, key.file);
    }
    template<class Component> void copyComponent(RE::TESAmmo* to, RE::TESAmmo* from)
    {
        static_cast<Component*>(to)->CopyComponent(static_cast<Component*>(from));
    }
    void copyAmmo(RE::TESAmmo* to, RE::TESAmmo* from, const std::string& name)
    {
        // TESAmmo does not override TESForm::Copy; copy its actual components.
        copyComponent<RE::TESModelTextureSwap>(to, from);
        copyComponent<RE::TESIcon>(to, from);
        copyComponent<RE::BGSMessageIcon>(to, from);
        copyComponent<RE::TESValueForm>(to, from);
        copyComponent<RE::TESWeightForm>(to, from);
        copyComponent<RE::BGSDestructibleObjectForm>(to, from);
        copyComponent<RE::BGSPickupPutdownSounds>(to, from);
        copyComponent<RE::TESDescription>(to, from);
        copyComponent<RE::BGSKeywordForm>(to, from);
        to->boundData = from->boundData;
        to->GetRuntimeData() = from->GetRuntimeData();
        to->fullName = name;
    }
    void disableSlots()
    {
        for (auto& slot : slots) {
            slot.original = nullptr; slot.poison = nullptr; slot.iconInfo = 0;
            if (slot.form) {
                slot.form->fullName = "Unavailable poisoned ammunition";
                slot.form->GetRuntimeData().data.flags.set(RE::AMMO_DATA::Flag::kNonPlayable);
            }
        }
    }
    bool restoreSlot(std::size_t i)
    {
        if (i >= recipes.size() || i >= slots.size()) return false;
        auto& slot = slots[i]; const auto& recipe = recipes[i];
        slot.original = nullptr; slot.poison = nullptr; slot.iconInfo = 0;
        auto base = resolve<RE::TESAmmo>(recipe.ammo);
        if (!base || !base->GetPlayable() || !base->GetRuntimeData().data.projectile ||
            !base->GetRuntimeData().data.projectile->IsArrow() || slotIDs.contains(base->GetFormID())) return false;
        RE::AlchemyItem* poison = nullptr;
        if (!recipe.poison.custom()) {
            poison = resolve<RE::AlchemyItem>(recipe.poison.source);
            if (!poison || !poison->IsPoison()) return false;
        } else {
            std::vector<RE::EffectSetting*> bases;
            std::vector<RE::BGSKeyword*> keywords;
            for (const auto& e : recipe.poison.effects) {
                auto effect = resolve<RE::EffectSetting>(e.base);
                if (!effect) return false;
                bases.push_back(effect);
            }
            for (const auto& k : recipe.poison.keywords) {
                auto keyword = resolve<RE::BGSKeyword>(k);
                if (!keyword) return false;
                keywords.push_back(keyword);
            }
            poison = slot.proxy;
            poison->effects.clear();
            poison->hostileCount = 0; poison->avEffectSetting = nullptr;
            poison->fullName = recipe.poison.name;
            poison->data.costOverride = recipe.poison.value;
            poison->data.flags = static_cast<RE::AlchemyItem::AlchemyFlag>(recipe.poison.flags);
            for (std::size_t e = 0; e < bases.size(); ++e) {
                const auto& value = recipe.poison.effects[e];
                auto effect = new RE::Effect();
                effect->baseEffect = bases[e];
                effect->effectItem.magnitude = value.magnitude;
                effect->effectItem.area = value.area; effect->effectItem.duration = value.duration;
                effect->cost = value.cost; effect->conditions.head = nullptr;
                effectArena.push_back(effect); poison->effects.push_back(effect);
                if (bases[e]->IsHostile()) ++poison->hostileCount;
            }
            auto keys = static_cast<RE::BGSKeywordForm*>(poison);
            keys->ClearDataComponent();
            if (!keywords.empty()) keys->AddKeywords(keywords);
        }
        copyAmmo(slot.form, base, recipe.name);
        slot.original = base; slot.poison = poison;
        slot.iconInfo = AmmoIcon::PackCoated({base->IsBolt(), AmmoIcon::NativeType(base), AmmoIcon::MagicType(poison, true)});
        SKSE::log::info("Slot {}: {:08X} <- {}|{:06X}; poison={}{}", i, slot.form->GetFormID(),
            recipe.ammo.file, recipe.ammo.local, recipe.poison.custom() ? "crafted: " : "static: ", recipe.poison.name);
        return true;
    }
    void restoreAll()
    {
        disableSlots();
        for (std::size_t i = 0; i < recipes.size(); ++i) if (!restoreSlot(i))
            SKSE::log::error("Slot {} unavailable: a source plugin/form is missing or changed. Slot is reserved, never recycled.", i);
    }
    std::optional<pa::Poison> snapshot(RE::AlchemyItem* item, pa::crafted::Issue* issue = nullptr)
    {
        return pa::crafted::capture(item, keyOf, displayName, issue);
    }
    void logSnapshotFailure(RE::AlchemyItem* item, const pa::crafted::Issue& issue)
    {
        SKSE::log::warn("Poison snapshot rejected: item={:08X} name='{}'; {}; component={} form={:08X}; detail='{}'",
            item ? item->GetFormID() : 0, displayName(item), pa::crafted::description(issue.reason),
            issue.index, issue.related, issue.detail);
    }
    std::int32_t count(RE::PlayerCharacter* player, RE::TESBoundObject* item)
    {
        auto inventory = player->GetInventoryCounts([&](RE::TESBoundObject& obj) { return &obj == item; });
        const auto it = inventory.find(item); return it != inventory.end() ? std::max(0, it->second) : 0;
    }
    bool safeInput(RE::PlayerCharacter* player, RE::TESBoundObject* item)
    {
        auto inventory = player->GetInventory([&](RE::TESBoundObject& obj) { return &obj == item; });
        const auto it = inventory.find(item);
        return it != inventory.end() && it->second.first > 0 && it->second.second &&
            !it->second.second->IsQuestObject() && it->second.second->IsOwnedBy(player);
    }
    // Resolve a live renamed bottle immediately before use. A present nullptr
    // means an ordinary copy without extra data; nullopt means no named match.
    // Never store the returned extra-data pointer in a queued Request.
    std::optional<RE::ExtraDataList*> namedBottle(RE::PlayerCharacter* player,
        RE::AlchemyItem* poison, const std::string& name)
    {
        const auto inventory = player->GetInventory([&](RE::TESBoundObject& obj) { return &obj == poison; });
        const auto found = inventory.find(poison);
        if (found == inventory.end() || found->second.first <= 0 || !found->second.second) return std::nullopt;
        const auto& entry = found->second.second;
        std::int32_t remainder = found->second.first;
        std::unordered_set<RE::ExtraDataList*> visited;
        const char* base = poison->GetName();
        if (entry->extraLists) for (auto* extra : *entry->extraLists) {
            if (!extra || !visited.insert(extra).second || remainder <= 0) continue;
            const auto copies = std::clamp(extra->GetCount(), 0, remainder);
            remainder -= copies;
            if (!copies) continue;
            const char* label = extra->GetDisplayName(poison);
            if (!label || !*label) label = base;
            if (label && name == label) return extra;
        }
        if (remainder > 0 && base && name == base) return static_cast<RE::ExtraDataList*>(nullptr);
        return std::nullopt;
    }
    struct Request
    {
        std::uint64_t epoch{};
        RE::FormID ammo{}, poison{}, weapon{};
        std::uint32_t doses{};
        pa::Recipe recipe;
        std::optional<std::string> inventoryName;
    };
    void refreshInventory()
    {
        if (auto ui = RE::UI::GetSingleton())
            if (auto menu = ui->GetMenu<RE::InventoryMenu>())
                if (auto list = menu->GetRuntimeData().itemList) list->Update(RE::PlayerCharacter::GetSingleton());
    }
    void craft(const Request& request, std::uint32_t bottles)
    {
        if (!session || request.epoch != generation || !bottles) return;
        auto player = RE::PlayerCharacter::GetSingleton();
        auto ammo = RE::TESForm::LookupByID<RE::TESAmmo>(request.ammo);
        auto poison = RE::TESForm::LookupByID<RE::AlchemyItem>(request.poison);
        if (!player || !ammo || !poison || player->GetCurrentAmmo() != ammo ||
            !player->GetEquippedObject(false) || player->GetEquippedObject(false)->GetFormID() != request.weapon) {
            notify("Poisoned Ammo: equipment changed. Select the poison again."); return;
        }
        pa::crafted::Issue snapshotIssue;
        const auto current = snapshot(poison, &snapshotIssue);
        if (!current) {
            logSnapshotFailure(poison, snapshotIssue);
            notify(fmt::format("Poisoned Ammo: {}. No items consumed.", pa::crafted::description(snapshotIssue.reason)));
            return;
        }
        if (*current != request.recipe.poison || !safeInput(player, ammo) || !safeInput(player, poison)) {
            notify("Poisoned Ammo: inputs changed, are stolen, or are quest items."); return;
        }
        const auto beforeAmmo = count(player, ammo), beforePoison = count(player, poison);
        const auto batch = pa::plan(beforeAmmo, beforePoison, request.doses, bottles, settings.maxBatch);
        if (!batch.arrows) { notify("Poisoned Ammo: not enough ammunition or poison."); return; }
        if (request.inventoryName && (batch.bottles != 1 || !namedBottle(player, poison, *request.inventoryName))) {
            notify("Poisoned Ammo: the selected named poison is no longer available."); return;
        }
        RE::TESAmmo* output = nullptr;
        {
            std::lock_guard lock(stateMutex);
            if (!formsReady || saveFault) return;
            auto slot = pa::existing(recipes, request.recipe);
            const bool newlyAssigned = !slot.has_value();
            if (!slot) {
                if (recipes.size() == pa::capacity) { notify("Poisoned Ammo: all 512 recipe slots are reserved in this save."); return; }
                recipes.push_back(request.recipe); slot = recipes.size() - 1;
            }
            if (!slots[*slot].poison && !restoreSlot(*slot)) {
                if (newlyAssigned) recipes.pop_back(); // No item referencing this new slot has existed yet.
                notify("Poisoned Ammo: source data unavailable; no items consumed."); return;
            }
            output = slots[*slot].form;
            marker->value = pa::fingerprint(recipes);
        }
        const auto beforeOutput = count(player, output);
        // No asynchronous work between validation, removal and output creation.
        std::int32_t removedPoison = 0;
        if (!request.inventoryName) {
            player->RemoveItem(poison, batch.bottles, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            removedPoison = std::clamp(beforePoison - count(player, poison), 0, batch.bottles);
            if (removedPoison != batch.bottles) {
                if (removedPoison) player->AddObjectToContainer(poison, nullptr, removedPoison, nullptr);
                notify("Poisoned Ammo: poison removal failed; batch cancelled."); return;
            }
        }
        player->RemoveItem(ammo, batch.arrows, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        auto removedAmmo = std::clamp(beforeAmmo - count(player, ammo), 0, batch.arrows);
        if (removedAmmo != batch.arrows) {
            if (removedAmmo) player->AddObjectToContainer(ammo, nullptr, removedAmmo, nullptr);
            if (removedPoison) player->AddObjectToContainer(poison, nullptr, removedPoison, nullptr);
            notify("Poisoned Ammo: ammunition removal failed; materials returned."); return;
        }
        player->AddObjectToContainer(output, nullptr, batch.arrows, nullptr);
        const auto added = std::clamp(count(player, output) - beforeOutput, 0, batch.arrows);
        if (added != batch.arrows) {
            if (added) player->RemoveItem(output, added, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
            player->AddObjectToContainer(ammo, nullptr, removedAmmo, nullptr);
            if (removedPoison) player->AddObjectToContainer(poison, nullptr, removedPoison, nullptr);
            notify("Poisoned Ammo: output creation failed; materials returned."); return;
        }
        if (request.inventoryName) {
            // A named Wheeler click always consumes exactly one bottle, last.
            // Earlier failures can therefore roll back without losing its name.
            const auto selected = namedBottle(player, poison, *request.inventoryName);
            const auto beforeSelectedRemoval = count(player, poison);
            if (selected) player->RemoveItem(poison, 1, RE::ITEM_REMOVE_REASON::kRemove, *selected, nullptr);
            if (!selected || beforeSelectedRemoval - count(player, poison) != 1) {
                player->RemoveItem(output, added, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                player->AddObjectToContainer(ammo, nullptr, removedAmmo, nullptr);
                notify("Poisoned Ammo: selected bottle unavailable; ammunition returned."); return;
            }
        }
        // Capture before EquipObject: it can emit reload events synchronously.
        const auto animationContext = pa::animation::capture(player, ammo, settings.autoEquip ? output : ammo);
        if (settings.autoEquip) if (auto equip = RE::ActorEquipManager::GetSingleton())
            equip->EquipObject(player, output, nullptr, 1, nullptr, false, true, false, true);
        refreshInventory();
        SKSE::log::info("Crafted {} x {} from {} bottles ({} arrows/bottle)", batch.arrows, request.recipe.name, batch.bottles, request.doses);
        notify(fmt::format("Created {} {}", batch.arrows, request.recipe.name));
        if (settings.immersiveAnimation) {
            try {
                pa::animation::completed(poison, batch.bottles,
                    [epoch = request.epoch] { return session && epoch == generation; }, settings.trace, animationContext);
            } catch (const std::exception& e) {
                // Animation failure must not report a completed batch as failed
                // or consume/refund materials a second time.
                SKSE::log::error("Completed batch; animation bridge failed: {}", e.what());
            }
        }
    }
    struct BatchCallback final : RE::IMessageBoxCallback
    {
        Request request;
        std::vector<std::uint32_t> options;
        std::atomic_bool used = false;
        BatchCallback(Request r, std::vector<std::uint32_t> values) : request(std::move(r)), options(std::move(values)) { unk0C = 0; }
        void Run(Message button) override
        {
            if (used.exchange(true)) return;
            const auto selected = static_cast<std::uint32_t>(button);
            const auto r = request;
            const auto n = selected < options.size() ? options[selected] : 0;
            SKSE::GetTaskInterface()->AddTask([r, n] {
                if (r.epoch != generation) return;
                menuPending = false;
                try { craft(r, n); } catch (const std::exception& e) { SKSE::log::error("Craft error: {}", e.what()); notify("Poisoned Ammo: crafting error. See the log."); }
            });
        }
    };
    bool showBatch(Request request, std::int32_t ammo, std::int32_t poisons)
    {
        auto factoryManager = RE::MessageDataFactoryManager::GetSingleton();
        auto strings = RE::InterfaceStrings::GetSingleton();
        if (!factoryManager || !strings) return false;
        auto factory = factoryManager->GetCreator<RE::MessageBoxData>(strings->messageBoxData);
        if (!factory) return false;
        auto box = factory->Create(); if (!box) return false;
        const auto maximum = pa::plan(ammo, poisons, request.doses, UINT32_MAX, settings.maxBatch);
        const auto ammunition = RE::TESForm::LookupByID<RE::TESAmmo>(request.ammo);
        const auto units = ammunition && ammunition->IsBolt() ? "bolts" : "arrows";
        const std::string body = fmt::format("Poison ammunition\n{}\n{} {} per bottle. Choose bottles to use.\nA partly used final bottle is consumed.", request.recipe.name, request.doses, units);
        box->bodyText = body.c_str();
        std::vector<std::uint32_t> options;
        for (auto n : {1u, 5u, 10u, static_cast<std::uint32_t>(maximum.bottles)}) {
            if (!n || n > static_cast<std::uint32_t>(maximum.bottles) || std::find(options.begin(), options.end(), n) != options.end()) continue;
            const auto p = pa::plan(ammo, poisons, request.doses, n, settings.maxBatch);
            const auto label = fmt::format("{} bottle{} / {} ammo", n, n == 1 ? "" : "s", p.arrows);
            options.push_back(n); box->buttonText.push_back(label.c_str());
        }
        options.push_back(0); box->buttonText.push_back("Cancel");
        box->callback.reset(new BatchCallback(std::move(request), std::move(options)));
        box->QueueMessage(); return true;
    }
    void requestCraft(RE::AlchemyItem* clickedPoison = nullptr, std::optional<std::string> inventoryName = std::nullopt)
    {
        if (!session || !formsReady) return;
        { std::lock_guard lock(stateMutex); if (saveFault) { notify("Poisoned Ammo: save data mismatch. Restore the matching .skse co-save."); return; } }
        auto ui = RE::UI::GetSingleton();
        auto player = RE::PlayerCharacter::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::InventoryMenu>() : nullptr;
        if (!ui || !player || (!clickedPoison && !menu) || ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) || menuPending.exchange(true)) return;
        auto poison = clickedPoison;
        if (!poison) {
            auto list = menu->GetRuntimeData().itemList;
            auto selected = list ? list->GetSelectedItem() : nullptr;
            auto entry = selected ? selected->data.objDesc : nullptr;
            poison = entry && entry->object ? entry->object->As<RE::AlchemyItem>() : nullptr;
        }
        auto ammo = player->GetCurrentAmmo();
        auto object = player->GetEquippedObject(false);
        auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
        const auto fail = [](const char* text) { menuPending = false; notify(text); };
        if (!poison || !poison->IsPoison()) { fail("Poisoned Ammo: highlight a poison in your inventory, then press the craft key."); return; }
        if (!ammo || !weapon || (!weapon->IsBow() && !weapon->IsCrossbow()) || weapon->IsCrossbow() != ammo->IsBolt()) {
            fail("Poisoned Ammo: equip a bow/crossbow and matching arrows/bolts first."); return;
        }
        if (slotIDs.contains(ammo->GetFormID()) || !ammo->GetPlayable()) { fail("Poisoned Ammo: equip ordinary ammunition first."); return; }
        if (!ammo->GetRuntimeData().data.projectile || !ammo->GetRuntimeData().data.projectile->IsArrow()) {
            fail("Poisoned Ammo: this ammunition does not use a native arrow/bolt projectile."); return;
        }
        const auto weaponEntry = player->GetEquippedEntryData(false);
        if (weaponEntry && weaponEntry->IsPoisoned()) { fail("Poisoned Ammo: use up the poison already on your bow first."); return; }
        const auto ammoKey = keyOf(ammo);
        if (!ammoKey) {
            SKSE::log::warn("Ammo snapshot rejected: {:08X} '{}'; no stable source record", ammo->GetFormID(), displayName(ammo));
            fail("Poisoned Ammo: this ammunition has no stable source record. No items consumed."); return;
        }
        pa::crafted::Issue snapshotIssue;
        const auto poisonData = snapshot(poison, &snapshotIssue);
        if (!poisonData) {
            logSnapshotFailure(poison, snapshotIssue);
            const auto text = fmt::format("Poisoned Ammo: {}. No items consumed.", pa::crafted::description(snapshotIssue.reason));
            fail(text.c_str()); return;
        }
        if (!safeInput(player, ammo) || !safeInput(player, poison)) { fail("Poisoned Ammo: quest items and stolen inputs cannot be used."); return; }
        float doses = 1.0f;
        if (settings.arrowsPerBottle) doses = static_cast<float>(settings.arrowsPerBottle);
        else {
            // Entry point 83 has owner, weapon, poison condition tabs, then float output.
            RE::BGSEntryPoint::HandleEntryPoint(RE::BGSEntryPoint::ENTRY_POINT::kModPoisonDoseCount,
                player, static_cast<RE::TESForm*>(weapon), static_cast<RE::TESForm*>(poison), &doses);
        }
        if (!std::isfinite(doses)) doses = 1;
        const auto baseDoses = static_cast<std::uint32_t>(std::clamp(doses, 1.0f, 10000.0f));
        const bool measured = coating::available && player->HasPerk(coating::measured);
        const auto perBottle = coating::doseCount(baseDoses, ammo->IsBolt(), measured);
        if (settings.trace) SKSE::log::info("Coating batch: base doses={}, bolts={}, Measured Dose={}, final doses={}",
            baseDoses, ammo->IsBolt(), measured, perBottle);
        Request request{generation.load(), ammo->GetFormID(), poison->GetFormID(), weapon->GetFormID(), perBottle,
            {*ammoKey, *poisonData, fmt::format("{} [{}]", displayName(ammo), poisonData->name)}, std::move(inventoryName)};
        if (!pa::valid(request.recipe)) { fail("Poisoned Ammo: unsupported poison data. No items consumed."); return; }
        const auto ammoCount = count(player, ammo), poisonCount = count(player, poison);
        if (!pa::plan(ammoCount, poisonCount, perBottle, 1, settings.maxBatch).arrows) {
            fail("Poisoned Ammo: no available batch."); return;
        }
        if (clickedPoison) {
            // Emergency coating: exactly one bottle, no message box or second
            // confirmation. Keep the gate held through the synchronous commit.
            struct Release { ~Release() { menuPending = false; } } release;
            craft(request, 1);
        } else if (!showBatch(std::move(request), ammoCount, poisonCount)) {
            fail("Poisoned Ammo: could not open the batch dialog.");
        }
    }

    // Called by Wheeler after its close animation. It can be on the render
    // thread: copy IDs/name and defer all inventory mutations to an SKSE task.
    std::uint32_t queueExternalCraft(std::uint32_t poisonID, const char* name,
        std::uint32_t weaponID, std::uint32_t ammoID)
    {
        if (!settings.craftOnUse) return 0;
        if (!session || !formsReady || externalPending.exchange(true)) return 1;
        const auto epoch = generation.load();
        try {
            std::optional<std::string> ownedName;
            if (name) ownedName = std::string(name);
            auto tasks = SKSE::GetTaskInterface();
            if (!tasks) { externalPending = false; return 1; }
            tasks->AddTask([epoch, poisonID, weaponID, ammoID, ownedName = std::move(ownedName)] {
                if (epoch != generation) return;
                struct Release { ~Release() { externalPending = false; } } release;
                if (!session || !formsReady) return;
                auto player = RE::PlayerCharacter::GetSingleton();
                auto weapon = player ? player->GetEquippedObject(false) : nullptr;
                auto ammo = player ? player->GetCurrentAmmo() : nullptr;
                if (!weapon || !ammo || weapon->GetFormID() != weaponID || ammo->GetFormID() != ammoID) {
                    notify("Poisoned Ammo: equipment changed or ammunition missing. Select the poison again."); return;
                }
                auto poison = RE::TESForm::LookupByID<RE::AlchemyItem>(poisonID);
                if (!poison || !poison->IsPoison()) { notify("Poisoned Ammo: selected poison is unavailable."); return; }
                try {
                    if (settings.trace) SKSE::log::info("Wheeler poison use -> one-bottle crafting: poison {:08X}, weapon {:08X}, ammo {:08X}", poisonID, weaponID, ammoID);
                    requestCraft(poison, ownedName);
                } catch (const std::exception& e) {
                    menuPending = false;
                    SKSE::log::error("Wheeler coating request failed: {}", e.what());
                    notify("Poisoned Ammo: coating error. See the log.");
                }
            });
        } catch (const std::exception& e) {
            if (epoch == generation) externalPending = false;
            SKSE::log::error("Could not queue Wheeler coating request: {}", e.what());
        }
        return 1; // Claimed even on refusal: never poison the bow as a fallback.
    }

    // The inventory's ItemSelect callback runs before vanilla starts weapon
    // poisoning or removes a bottle. SkyUI sends this for mouse use and its
    // AttemptEquip keyboard/controller action. Forward every other item/use.
    // No SWF replacement or native poison-function prologue patch is needed.
    struct InventoryUseHook
    {
        using Callback = RE::FxDelegateHandler::CallbackFn;
        using Processor = RE::FxDelegateHandler::CallbackProcessor;
        inline static REL::Relocation<void (*)(RE::InventoryMenu*, Processor*)> accept;
        inline static std::array<Callback*, 32> callbacks{};
        inline static std::size_t count{};

        static bool redirect(const RE::FxDelegateArgs& args)
        {
            if (!settings.craftOnUse || !session || !formsReady) return false;
            auto ui = RE::UI::GetSingleton();
            auto player = RE::PlayerCharacter::GetSingleton();
            auto menu = ui ? ui->GetMenu<RE::InventoryMenu>() : nullptr;
            if (!menu || !player || args.GetHandler() != menu.get()) return false;
            auto list = menu->GetRuntimeData().itemList;
            auto selected = list ? list->GetSelectedItem() : nullptr;
            auto entry = selected ? selected->data.objDesc : nullptr;
            auto poison = entry && entry->object ? entry->object->As<RE::AlchemyItem>() : nullptr;
            auto object = player->GetEquippedObject(false);
            auto weapon = object ? object->As<RE::TESObjectWEAP>() : nullptr;
            if (!poison || !poison->IsPoison() || !weapon || (!weapon->IsBow() && !weapon->IsCrossbow())) return false;
            // Capture and commit this click synchronously so a highlight change
            // cannot select another poison. F8 alone opens batch selection.
            if (settings.trace) SKSE::log::info("Inventory poison use -> ammo crafting: poison {:08X}; weapon {:08X}",
                poison->GetFormID(), weapon->GetFormID());
            try { requestCraft(poison); }
            catch (const std::exception& e) {
                menuPending = false;
                SKSE::log::error("Inventory coating request failed: {}", e.what());
                notify("Poisoned Ammo: coating error. See the log.");
            }
            // Invalid ammo, occupied dialog, or failed input validation
            // must never fall through into vanilla bow poisoning.
            return true;
        }
        template<std::size_t I> static void Select(const RE::FxDelegateArgs& args)
        {
            if (!redirect(args)) callbacks[I](args);
        }
        template<std::size_t... I> static constexpr auto wrappers(std::index_sequence<I...>)
        {
            return std::array<Callback*, sizeof...(I)>{&Select<I>...};
        }
        class Proxy final : public Processor
        {
            Processor* real;
        public:
            explicit Proxy(Processor* value) : real(value) {}
            void Process(const RE::GString& name, Callback* callback) override
            {
                if (name != "ItemSelect" || !callback) { real->Process(name, callback); return; }
                static constexpr auto functions = wrappers(std::make_index_sequence<32>{});
                if (std::find(functions.begin(), functions.end(), callback) != functions.end()) {
                    real->Process(name, callback); return; // Already wrapped by a chained registrar.
                }
                std::size_t index = 0;
                while (index < count && callbacks[index] != callback) ++index;
                if (index == count && count < callbacks.size()) {
                    callbacks[count++] = callback;
                    SKSE::log::info("Inventory ItemSelect coating route registered (chain {})", index);
                }
                if (index == callbacks.size()) SKSE::log::error("Inventory callback capacity reached; this callback retains native behavior");
                real->Process(name, index < callbacks.size() ? functions[index] : callback);
            }
        };
        static void Accept(RE::InventoryMenu* menu, Processor* processor)
        {
            Proxy proxy(processor); accept(menu, &proxy);
        }
        static void install()
        {
            REL::Relocation<std::uintptr_t> table{RE::VTABLE_InventoryMenu[0]};
            accept = table.write_vfunc(0x01, Accept);
        }
    };
    struct Input final : RE::BSTEventSink<RE::InputEvent*>
    {
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events, RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (events && session) for (auto e = *events; e; e = e->next) {
                const auto button = e->AsButtonEvent();
                if (button && button->device == RE::INPUT_DEVICE::kKeyboard && button->IsDown() && button->GetIDCode() == settings.key) {
                    const auto epoch = generation.load();
                    SKSE::GetTaskInterface()->AddTask([epoch] { if (epoch == generation) requestCraft(); });
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } input;

    void prepareProjectile(RE::ArrowProjectile* projectile, bool actorContact)
    {
        if (!session || !projectile) return;
        auto& runtime = projectile->GetProjectileRuntimeData();
        if (!runtime.ammoSource) return;
        Slot slot;
        {
            std::lock_guard lock(stateMutex);
            auto it = slotIDs.find(runtime.ammoSource->GetFormID());
            if (it == slotIDs.end() || saveFault) return;
            slot = slots[it->second];
        }
        if (!slot.poison || !slot.original) return;
        auto& arrow = projectile->GetArrowRuntimeData();
        const bool fresh = arrow.poison != slot.poison;
        arrow.poison = slot.poison;
        // Native impact handling applies the poison. Recovery must use the base ammo
        // on any actor contact; world misses retain the stable poisoned ammo record.
        if (actorContact) runtime.ammoSource = slot.original;
        if (settings.trace && (fresh || actorContact)) SKSE::log::info("Projectile {:08X}: native poison {:08X}, actor contact={}, recover {:08X}",
            projectile->GetFormID(), slot.poison->GetFormID(), actorContact, runtime.ammoSource->GetFormID());
    }
    struct LoadedHook
    {
        static void thunk(RE::ArrowProjectile* p) { original(p); prepareProjectile(p, false); }
        static inline REL::Relocation<decltype(thunk)> original;
    };
    struct ImpactHook
    {
        // Native slot 0xBD returns an ImpactData pointer. The pinned CommonLib
        // header incorrectly declares void. Retain RAX across Scope cleanup:
        // the collision caller writes into the returned impact at offset 0x48.
        // See CRASH_FIX.md for the independent ABI reference and crash evidence.
        static RE::Projectile::ImpactData* thunk(RE::ArrowProjectile* p, RE::TESObjectREFR* target,
            const RE::NiPoint3& point, const RE::NiPoint3& velocity,
            RE::hkpCollidable* collidable, std::uint32_t shapeKey, bool spellCollided)
        {
            prepareProjectile(p, target && target->As<RE::Actor>());
            coating::Scope scope(p, session.load());
            return original(p, target, point, velocity, collidable, shapeKey, spellCollided);
        }
        static inline REL::Relocation<decltype(thunk)> original;
    };
    struct ProcessHook
    {
        static bool thunk(RE::ArrowProjectile* p)
        {
            bool actorContact = false;
            for (auto impact : p->GetProjectileRuntimeData().impacts) if (impact) {
                const auto ref = impact->collidee.get();
                if (ref && ref->As<RE::Actor>()) { actorContact = true; break; }
            }
            prepareProjectile(p, actorContact);
            coating::Scope scope(p, session.load());
            return original(p);
        }
        static inline REL::Relocation<decltype(thunk)> original;
    };
    void installHooks()
    {
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_ArrowProjectile[0]};
        LoadedHook::original = table.write_vfunc(0xC0, LoadedHook::thunk);
        ImpactHook::original = table.write_vfunc(0xBD, ImpactHook::thunk);
        ProcessHook::original = table.write_vfunc(0xAC, ProcessHook::thunk);
        SKSE::log::info("ArrowProjectile hooks installed; AddImpact ABI returns ImpactData*. Chained loaded/impact/process functions.");
    }
    void reset()
    {
        session = false; menuPending = false; externalPending = false; ++generation;
        std::lock_guard lock(stateMutex);
        recipes.clear(); saveFault = false; disableSlots();
    }
    void save(SKSE::SerializationInterface* api)
    {
        std::lock_guard lock(stateMutex);
        if (!formsReady || saveFault) { SKSE::log::error("Save blocked: poisoned ammo state unavailable; ESS marker retained"); return; }
        try {
            const auto bytes = pa::encode(recipes);
            if (!api->WriteRecord(recordID, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
                SKSE::log::error("Could not write poison recipe co-save");
            else SKSE::log::info("Saved {} recipe slots, {} bytes", recipes.size(), bytes.size());
        } catch (const std::exception& e) { SKSE::log::error("Save error: {}", e.what()); }
    }
    void load(SKSE::SerializationInterface* api)
    {
        std::lock_guard lock(stateMutex);
        recipes.clear(); disableSlots(); saveFault = false;
        std::uint32_t type{}, version{}, length{}; bool found = false;
        while (api->GetNextRecordInfo(type, version, length)) {
            if (type != recordID) continue;
            try {
                if (found || version != 1 || length > pa::maxSaveBytes) throw std::runtime_error("duplicate/unsupported recipe record");
                found = true;
                std::vector<std::uint8_t> bytes(length);
                if (api->ReadRecordData(bytes.data(), length) != length) throw std::runtime_error("short recipe record");
                recipes = pa::decode(bytes);
            } catch (const std::exception& e) { saveFault = true; SKSE::log::error("Co-save error: {}", e.what()); }
        }
        if (!saveFault && formsReady) restoreAll();
        SKSE::log::info("Loaded {} recipe slots; record found={}, fault={}", recipes.size(), found, saveFault);
    }
    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded: {
            std::lock_guard lock(stateMutex);
            settings.load(); auto data = RE::TESDataHandler::GetSingleton();
            marker = data->LookupForm<RE::TESGlobal>(pa::markerID, pa::plugin);
            formsReady = marker != nullptr;
            for (std::size_t i = 0; i < slots.size(); ++i) {
                auto& slot = slots[i];
                slot.form = data->LookupForm<RE::TESAmmo>(pa::ammoStart + static_cast<std::uint32_t>(i), pa::plugin);
                slot.proxy = data->LookupForm<RE::AlchemyItem>(pa::poisonStart + static_cast<std::uint32_t>(i), pa::plugin);
                formsReady = formsReady && slot.form && slot.proxy;
                if (slot.form) slotIDs.emplace(slot.form->GetFormID(), i);
            }
            if (!formsReady) { SKSE::log::error("Disabled: enable the matching PoisonedAmmoNative.esp"); break; }
            disableSlots(); installHooks(); coating::install(data, settings.trace);
            InventoryUseHook::install();
            if (settings.immersiveAnimation) pa::animation::install();
            RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&input);
            SKSE::log::info("Ready: {} stable ESL slots; key {}; dose override {}; auto-equip {}; craft on inventory poison use {}", slots.size(), settings.key, settings.arrowsPerBottle, settings.autoEquip, settings.craftOnUse);
            break;
        }
        case SKSE::MessagingInterface::kPreLoadGame: session = false; menuPending = false; externalPending = false; ++generation; break;
        case SKSE::MessagingInterface::kNewGame: {
            reset(); if (marker) marker->value = 0; session = formsReady; break;
        }
        case SKSE::MessagingInterface::kPostLoadGame: {
            if (!message->data || !formsReady) break;
            std::lock_guard lock(stateMutex);
            if (saveFault || marker->value != pa::fingerprint(recipes)) {
                saveFault = true; disableSlots();
                SKSE::log::error("ESS/co-save mismatch: marker={}, recipes={}. Crafting and poison delivery disabled.", marker->value, recipes.size());
                SKSE::GetTaskInterface()->AddTask([] { RE::DebugMessageBox("Poisoned Ammo: this save's .skse co-save is missing, damaged, or mismatched. Restore the matching ESS and SKSE files. Crafting is disabled to protect existing ammunition."); });
            }
            session = true;
            break;
        }
        default: break;
        }
    }
}
// Read-only icon query. Copy only a tagged integer across the DLL boundary.
// Never expose recipe/effect pointers or mutate inventory on Wheeler's thread.
extern "C" __declspec(dllexport) std::uint32_t PoisonedAmmoNative_GetIconInfoV1(std::uint32_t ammoID) noexcept
{
    try {
        std::lock_guard lock(stateMutex);
        if (!session || !formsReady || saveFault) return 0;
        const auto found = slotIDs.find(ammoID);
        if (found == slotIDs.end()) return 0;
        const auto& slot = slots[found->second];
        return slot.poison && slot.original ? slot.iconInfo : 0;
    } catch (...) { return 0; }
}
extern "C" __declspec(dllexport) std::uint32_t PoisonedAmmoNative_CoatOneV1(
    std::uint32_t poison, const char* inventoryName, std::uint32_t clickedWeapon, std::uint32_t clickedAmmo)
{
    return queueExternalCraft(poison, inventoryName, clickedWeapon, clickedAmmo);
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({0, 2, 6, 0}); data.PluginName("PoisonedAmmoNative");
    data.AuthorName("Physics-helper contributors"); data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1, 6, 1170, 0}}); return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != REL::Version{1, 6, 1170, 0}) return false;
    auto path = SKSE::log::log_directory(); if (!path) return false;
    *path /= "PoisonedAmmoNative.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("PoisonedAmmoNative 0.2.4 beta; read-only elemental/coating icon metadata; Skyrim Steam 1.6.1170; impact pointer ABI fixed; Inventory/Wheeler click coats one bottle; F8 opens batch selection");
    auto api = SKSE::GetSerializationInterface(); api->SetUniqueID(saveID);
    api->SetSaveCallback(save); api->SetLoadCallback(load);
    api->SetRevertCallback([](SKSE::SerializationInterface*) { reset(); });
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
