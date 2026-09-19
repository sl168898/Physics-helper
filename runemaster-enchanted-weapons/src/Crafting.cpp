#include "Crafting.h"
#include "CraftingRules.h"
#include <SKSE/SKSE.h>
#include <intrin.h>
#include <memory>
#include <mutex>

namespace runes::crafting {
namespace {
struct Recipe { RE::TESObjectWEAP *input{}, *output{}; RE::BGSConstructibleObject* form{}; };
std::array<Recipe, 8> recipes;
bool (*enabled)() = nullptr;
bool installed = false;
thread_local bool insideRemove = false, insideAdd = false;
// RemoveItem returns a non-trivial handle by value. The MSVC member-function
// ABI places its hidden return buffer AFTER this, unlike a free function.
// Preserve that argument explicitly both in our thunk and the chained call.
using Remove = RE::ObjectRefHandle* (*)(RE::PlayerCharacter*, RE::ObjectRefHandle*, RE::TESBoundObject*, std::int32_t,
    RE::ITEM_REMOVE_REASON, RE::ExtraDataList*, RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);
using Add = void (*)(RE::PlayerCharacter*, RE::TESBoundObject*, RE::ExtraDataList*, std::int32_t, RE::TESObjectREFR*);
REL::Relocation<Remove> originalRemove;
REL::Relocation<Add> originalAdd;
std::mutex mutex;
std::uint64_t generation = 0;
bool queued = false;
bool closing = false;

// Same dynamic-enchantment retain mechanism used by SKSE's
// PersistentFormManager::IncRefEnchantment. Never change the effect definition.
bool reference(RE::EnchantmentItem* enchantment, bool retain) {
    if (!enchantment || enchantment->GetFormID() < 0xFF000000) return true;
    auto manager = RE::BGSCreatedObjectManager::GetSingleton();
    if (!manager) return false;
    RE::BSSpinLockGuard lock(manager->lock);
    for (auto& entry : manager->weaponEnchantments) {
        if (entry.magicItem != enchantment) continue;
        auto count = reinterpret_cast<volatile long*>(&entry.refCount);
        if (retain) _InterlockedIncrement(count);
        else _InterlockedDecrement(count);
        // Do not introduce deletion work for forms owned by the game. The
        // consumed item's enchantment can still be referenced by another mod.
        return true;
    }
    return false;
}
using EnchantmentRef = std::shared_ptr<RE::EnchantmentItem>;
EnchantmentRef hold(RE::EnchantmentItem* enchantment) {
    if (!enchantment || !reference(enchantment, true)) return {};
    return EnchantmentRef(enchantment, [](RE::EnchantmentItem* value) { reference(value, false); });
}
struct Snapshot {
    struct ExtraStack { std::uintptr_t address; std::int32_t count; };
    bool valid = true;
    std::int32_t total{}, plain{};
    std::vector<Stack> stacks;
    std::vector<EnchantmentRef> retained;
    std::vector<ExtraStack> extraStacks;
};
Snapshot inventory(RE::PlayerCharacter* player, RE::TESObjectWEAP* weapon, bool retain) {
    Snapshot result;
    const auto items = player->GetInventory([weapon](RE::TESBoundObject& item) { return &item == weapon; });
    const auto found = items.find(weapon);
    if (found == items.end()) return result;
    result.total = found->second.first;
    result.plain = result.total;
    if (result.total < 0) { result.valid = false; return result; }
    const auto& entry = found->second.second;
    if (entry && entry->extraLists) {
        for (auto list : *entry->extraLists) {
            if (!list) continue;
            const auto count = list->GetCount();
            if (count < 0) { result.valid = false; return result; }
            result.plain -= count;
            result.extraStacks.push_back({reinterpret_cast<std::uintptr_t>(list), count});
            Identity value;
            auto extra = list->GetByType<RE::ExtraEnchantment>();
            auto enchantment = extra && extra->enchantment ? extra->enchantment : weapon->formEnchanting;
            if (enchantment) {
                value.enchantment = enchantment->GetFormID();
                value.capacity = extra ? extra->charge : weapon->amountofEnchantment;
                value.temporary = extra && extra->removeOnUnequip;
                auto charge = list->GetByType<RE::ExtraCharge>();
                value.charge = charge ? charge->charge : static_cast<float>(value.capacity);
                if (retain) {
                    auto retained = hold(enchantment);
                    if (!retained) { result.valid = false; return result; }
                    result.retained.push_back(std::move(retained));
                }
            }
            if (auto health = list->GetByType<RE::ExtraHealth>()) value.health = health->health;
            if (auto name = list->GetByType<RE::ExtraTextDisplayData>(); name && name->IsPlayerSet())
                value.name = name->displayName.c_str();
            append(result.stacks, std::move(value), count);
        }
    }
    if (result.plain < 0) { result.valid = false; return result; }
    if (result.plain) {
        Identity value;
        if (auto enchantment = weapon->formEnchanting) {
            value.enchantment = enchantment->GetFormID();
            value.capacity = weapon->amountofEnchantment;
            value.charge = static_cast<float>(value.capacity);
            if (retain) {
                auto retained = hold(enchantment);
                if (!retained) { result.valid = false; return result; }
                result.retained.push_back(std::move(retained));
            }
        }
        append(result.stacks, std::move(value), result.plain);
    }
    return result;
}
struct Consumed { Identity identity; EnchantmentRef enchantment; };
struct Output { std::uintptr_t extraAddress{}; bool identified{}; };
struct Transaction {
    std::vector<Consumed> consumed;
    std::vector<Output> outputs;
    std::size_t plainOutputs{}, signals{};
    std::int32_t lastPlainAfter{};
};
std::array<Transaction, 8> pending;

bool forge() {
    if (!enabled || !enabled() || RE::TaskQueueInterface::ShouldUseTaskQueue()) return false;
    auto ui = RE::UI::GetSingleton();
    auto menu = ui ? ui->GetMenu<RE::CraftingMenu>() : nullptr;
    auto sub = menu ? menu->GetCraftingSubMenu() : nullptr;
    return sub && skyrim_cast<RE::CraftingSubMenus::ConstructibleObjectMenu*>(sub);
}
bool selectedRecipe(int index) {
    if (!forge()) return false;
    auto menu = RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();
    auto sub = menu ? skyrim_cast<RE::CraftingSubMenus::ConstructibleObjectMenu*>(menu->GetCraftingSubMenu()) : nullptr;
    return sub && sub->currentCobjIdx < sub->crafts.size() &&
        sub->crafts[sub->currentCobjIdx].constructibleObject == recipes[index].form;
}
int inputIndex(RE::TESBoundObject* item) {
    int result = -1;
    for (int i = 0; i < 8; ++i) {
        if (!recipes[i].input || recipes[i].input != item) continue;
        if (result != -1) return -1;  // Ambiguous custom recipe inputs: do not guess.
        result = i;
    }
    return result;
}
int outputIndex(RE::TESForm* item) {
    for (int i = 0; i < 8; ++i) if (recipes[i].output && recipes[i].output == item) return i;
    return -1;
}
RE::InventoryEntryData* liveEntry(RE::PlayerCharacter* player, RE::TESBoundObject* item) {
    auto changes = player->GetInventoryChanges();
    if (changes && changes->entryList)
        for (auto entry : *changes->entryList) if (entry && entry->object == item) return entry;
    return nullptr;
}
RE::ExtraDataList* createExtraList() {
    // AE 1.6.629+ adds a BaseExtraList vtable. CommonLib's portable declaration
    // is opaque on AE: sizeof(ExtraDataList) and its C++ new/delete are not usable.
    // Allocate the real 1.6.1170 size and run Skyrim's constructor, which also
    // initializes the vtable and inventory-list lock.
    auto memory = RE::malloc(0x20);
    if (!memory) return nullptr;
    using Constructor = RE::ExtraDataList* (*)(void*);
    REL::Relocation<Constructor> construct{RELOCATION_ID(11437, 11583)};
    return construct(memory);
}
bool transfer(RE::PlayerCharacter* player, RE::TESObjectWEAP* output, const Consumed& item, const Output& evidence) {
    if (!transferable(item.identity)) return true;
    auto entry = liveEntry(player, output);
    if (!entry || !item.enchantment || output->formEnchanting) return false;
    RE::ExtraDataList* list = nullptr;
    if (evidence.extraAddress) {
        if (entry->extraLists) for (auto candidate : *entry->extraLists)
            if (reinterpret_cast<std::uintptr_t>(candidate) == evidence.extraAddress) { list = candidate; break; }
        if (!list || list->GetCount() != 1) return false;
        if (auto existing = list->GetByType<RE::ExtraEnchantment>(); existing && existing->enchantment) {
            // Another crafting add-on may already have transferred it.
            return existing->enchantment == item.enchantment.get() && existing->charge == item.identity.capacity;
        }
    } else {
        const auto now = inventory(player, output, false);
        if (!now.valid || now.plain < 1) return false;
    }
    if (!reference(item.enchantment.get(), true)) return false;
    if (!list) {
        list = createExtraList();
        if (!list) { reference(item.enchantment.get(), false); return false; }
    }
    list->Add(new RE::ExtraEnchantment(item.enchantment.get(), item.identity.capacity, false));
    if (auto charge = list->GetByType<RE::ExtraCharge>()) charge->charge = item.identity.charge;
    else {
        auto newCharge = new RE::ExtraCharge(); newCharge->charge = item.identity.charge; list->Add(newCharge);
    }
    if (item.identity.health != 1.f) {
        if (auto health = list->GetByType<RE::ExtraHealth>()) health->health = item.identity.health;
        else list->Add(new RE::ExtraHealth(item.identity.health));
    }
    if (!item.identity.name.empty()) {
        if (auto text = list->GetByType<RE::ExtraTextDisplayData>()) text->SetName(item.identity.name.c_str());
        else list->Add(new RE::ExtraTextDisplayData(item.identity.name.c_str()));
    }
    // Convert one newly crafted unextended copy to an extended inventory copy.
    // countDelta already includes the craft; never add or remove another item.
    if (!evidence.extraAddress) entry->AddExtraList(list);
    player->GetInventoryChanges()->changed = true;
    player->AddChange(RE::TESObjectREFR::ChangeFlags::kInventory);
    SKSE::log::info("Preserved crafted enchantment {:08X} on {:08X}; maximum charge={}; current charge={}",
        item.identity.enchantment, output->GetFormID(), item.identity.capacity, item.identity.charge);
    return true;
}
void flush(std::uint64_t epoch) {
    std::array<Transaction, 8> transactions;
    const bool menuOpen = forge();
    {
        std::lock_guard lock(mutex);
        if (epoch != generation) return;
        queued = false;
        for (int i = 0; i < 8; ++i) {
            const auto& part = pending[i];
            const bool complete = part.signals && part.consumed.size() >= part.signals && part.outputs.size() >= part.signals;
            // Ingredient/output/story notifications may arrive on different
            // task passes. Wait for their matching signal, or the menu to close.
            if (!complete && menuOpen && !closing) continue;
            transactions[i] = std::move(pending[i]);
            pending[i] = {};
        }
    }
    auto player = RE::PlayerCharacter::GetSingleton();
    if (!enabled || !enabled() || !player) return;
    for (int i = 0; i < 8; ++i) {
        auto& transaction = transactions[i];
        if (transaction.outputs.empty() && transaction.consumed.empty() && !transaction.signals) continue;
        auto now = inventory(player, recipes[i].output, false);
        const bool identified = std::all_of(transaction.outputs.begin(), transaction.outputs.end(), [](const auto& out) { return out.identified; });
        if (!now.valid || !matched(transaction.consumed.size(), transaction.outputs.size(), transaction.signals,
            identified, transaction.plainOutputs, now.plain, transaction.lastPlainAfter)) {
            SKSE::log::warn("Unmatched rune crafting transaction {:08X}: consumed={}, outputs={}, crafts={}; inventory left untouched",
                recipes[i].output->GetFormID(), transaction.consumed.size(), transaction.outputs.size(), transaction.signals);
            continue;
        }
        for (std::size_t n = 0; n < transaction.consumed.size(); ++n) {
            if (!transfer(player, recipes[i].output, transaction.consumed[n], transaction.outputs[n]))
                SKSE::log::error("Could not attach the consumed weapon's enchantment to {:08X}", recipes[i].output->GetFormID());
        }
    }
}
void queue() {  // Caller holds mutex; engine calls happen only in flush, outside it.
    if (queued) return;
    queued = true;
    const auto epoch = generation;
    SKSE::GetTaskInterface()->AddTask([epoch] { flush(epoch); });
}
RE::ObjectRefHandle* removeHook(RE::PlayerCharacter* player, RE::ObjectRefHandle* returnBuffer,
    RE::TESBoundObject* item, std::int32_t count,
    RE::ITEM_REMOVE_REASON reason, RE::ExtraDataList* extra, RE::TESObjectREFR* destination,
    const RE::NiPoint3* drop, const RE::NiPoint3* rotation) {
    const auto index = inputIndex(item);
    if (insideRemove || index < 0 || count != 1 || destination || !selectedRecipe(index))
        return originalRemove(player, returnBuffer, item, count, reason, extra, destination, drop, rotation);
    struct Guard { Guard() { insideRemove = true; } ~Guard() { insideRemove = false; } } guard;
    auto before = inventory(player, recipes[index].input, true);
    auto result = originalRemove(player, returnBuffer, item, count, reason, extra, destination, drop, rotation);
    auto after = inventory(player, recipes[index].input, false);
    if (!before.valid || !after.valid || before.total - after.total != 1) return result;
    auto lost = singleLoss(before.stacks, after.stacks);
    if (!lost) {
        SKSE::log::warn("Ambiguous consumed ebony weapon {:08X}; enchantment transfer skipped", item->GetFormID());
        return result;
    }
    Consumed consumed{*lost, {}};
    if (lost->enchantment) {
        for (auto& retained : before.retained)
            if (retained && retained->GetFormID() == lost->enchantment) { consumed.enchantment = retained; break; }
        if (!consumed.enchantment) return result;
    }
    {
        std::lock_guard lock(mutex);
        pending[index].consumed.push_back(std::move(consumed));
        queue();
    }
    return result;
}
void addHook(RE::PlayerCharacter* player, RE::TESBoundObject* item, RE::ExtraDataList* extra,
    std::int32_t count, RE::TESObjectREFR* source) {
    const auto index = outputIndex(item);
    if (insideAdd || index < 0 || count != 1 || source || !selectedRecipe(index)) {
        originalAdd(player, item, extra, count, source); return;
    }
    struct Guard { Guard() { insideAdd = true; } ~Guard() { insideAdd = false; } } guard;
    auto before = inventory(player, recipes[index].output, false);
    originalAdd(player, item, extra, count, source);
    auto after = inventory(player, recipes[index].output, false);
    if (!before.valid || !after.valid || after.total - before.total != 1) return;
    Output evidence;
    if (after.plain - before.plain == 1) evidence.identified = true;
    else {
        std::vector<std::uintptr_t> created;
        for (auto now : after.extraStacks) {
            bool existed = false;
            for (auto old : before.extraStacks) if (old.address == now.address) { existed = true; break; }
            if (!existed && now.count == 1) created.push_back(now.address);
        }
        if (created.size() == 1) evidence = {created.front(), true};
    }
    std::lock_guard lock(mutex);
    auto& transaction = pending[index];
    transaction.outputs.push_back(evidence);
    if (evidence.identified && !evidence.extraAddress) {
        ++transaction.plainOutputs;
        transaction.lastPlainAfter = after.plain;
    }
    queue();
}
class Events final : public RE::BSTEventSink<RE::ItemCrafted::Event>,
                     public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::ItemCrafted::Event* event,
        RE::BSTEventSource<RE::ItemCrafted::Event>*) override {
        if (!event || !event->item || !forge()) return RE::BSEventNotifyControl::kContinue;
        const auto index = outputIndex(event->item);
        if (index < 0) return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(mutex);
        ++pending[index].signals;
        queue();
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (!event || event->menuName != RE::CraftingMenu::MENU_NAME) return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(mutex);
        closing = !event->opening;
        if (closing) queue();
        return RE::BSEventNotifyControl::kContinue;
    }
} events;
}
void install(const std::array<RE::BGSConstructibleObject*, 8>& forms, bool (*isEnabled)()) {
    if (installed) return;
    enabled = isEnabled;
    for (int i = 0; i < 8; ++i) {
        auto recipe = forms[i];
        if (!recipe || !recipe->createdItem || recipe->data.numConstructed != 1) continue;
        auto output = recipe->createdItem->As<RE::TESObjectWEAP>();
        RE::TESObjectWEAP* input = nullptr;
        bool valid = output != nullptr;
        for (std::uint32_t j = 0; j < recipe->requiredItems.numContainerObjects; ++j) {
            auto ingredient = recipe->requiredItems.containerObjects[j];
            auto weapon = ingredient && ingredient->obj ? ingredient->obj->As<RE::TESObjectWEAP>() : nullptr;
            if (!weapon) continue;
            if (input || ingredient->count != 1) { valid = false; break; }
            input = weapon;
        }
        if (valid && input && input != output) {
            recipes[i] = {input, output, recipe};
            SKSE::log::info("Crafting preservation: recipe={:08X}; input={:08X}; output={:08X}",
                recipe->GetFormID(), input->GetFormID(), output->GetFormID());
        } else SKSE::log::warn("Unsupported custom rune recipe {:08X}; crafting transfer disabled for it", recipe->GetFormID());
    }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
    originalRemove = table.write_vfunc(0x56, removeHook);
    originalAdd = table.write_vfunc(0x5A, addHook);
    RE::ItemCrafted::GetEventSource()->AddEventSink(&events);
    RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
    installed = true;
    SKSE::log::info("Installed consumed-instance snapshots and verified crafting output transfer");
}
void reset() {
    std::array<Transaction, 8> discarded;
    {
        std::lock_guard lock(mutex);
        ++generation;
        queued = false;
        closing = false;
        discarded = std::move(pending);
        pending = {};
    }  // Release temporary form references without holding the transaction mutex.
}
}
