#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <MinHook.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <mutex>
#include <set>
#include "Bindings.h"

namespace {
constexpr std::uint32_t kSerialization = 0x45534431; // ESD1
constexpr std::uint32_t kBindingRecord = 0x42494E44; // BIND
std::mutex mutex;
esd::Bindings bindings;
esd::Catalog catalog;
std::set<std::pair<RE::FormID, RE::FormID>> loggedCards;
std::atomic_bool ready = false;
using Populate = void (*)(RE::ItemCard*, RE::InventoryEntryData*, bool);
Populate originalPopulate = nullptr;

bool supported(RE::TESBoundObject* object) {
    return object && (object->As<RE::TESObjectWEAP>() || object->As<RE::TESObjectARMO>());
}
RE::EnchantmentItem* baseEnchantment(RE::TESBoundObject* object) {
    if (!object) return nullptr;
    if (auto* weapon = object->As<RE::TESObjectWEAP>()) return weapon->formEnchanting;
    if (auto* armor = object->As<RE::TESObjectARMO>()) return armor->formEnchanting;
    return nullptr;
}
std::string description(RE::TESBoundObject* object) {
    RE::BSString text;
    if (object) {
        if (auto* weapon = object->As<RE::TESObjectWEAP>()) weapon->GetDescription(text, weapon);
        else if (auto* armor = object->As<RE::TESObjectARMO>()) armor->GetDescription(text, armor);
    }
    return text.c_str() ? text.c_str() : "";
}
esd::Key keyFor(RE::TESBoundObject* object, RE::ExtraDataList* extras) {
    const auto* uid = extras ? extras->GetByType<RE::ExtraUniqueID>() : nullptr;
    if (!object || !uid) return {};
    return {object->GetFormID(), uid->baseID, uid->uniqueID};
}
RE::TESBoundObject* sourceFor(RE::TESBoundObject* object, RE::ExtraDataList* extras,
    RE::EnchantmentItem* enchantment, bool allowRecovery) {
    if (!supported(object) || !enchantment) return nullptr;
    RE::FormID source = 0;
    {
        std::lock_guard lock(mutex);
        if (auto binding = bindings.find(keyFor(object, extras), enchantment->GetFormID())) source = binding->source;
        else if (allowRecovery) source = catalog.find(enchantment->GetFormID(), object->As<RE::TESObjectWEAP>() != nullptr);
    }
    auto* donor = RE::TESForm::LookupByID<RE::TESBoundObject>(source);
    // Reject stale or removed source records after load-order/mod changes.
    return supported(donor) && baseEnchantment(donor) == enchantment ? donor : nullptr;
}

// Runs once for each successful transfer, while the original mod still holds
// both dropped references. Only description metadata is touched here.
bool remember(RE::StaticFunctionTag*, RE::TESObjectREFR* donor, RE::TESObjectREFR* recipient,
    RE::EnchantmentItem* enchantment) {
    if (!ready || !donor || !recipient || donor == recipient || !enchantment) return false;
    auto* sourceObject = donor->GetBaseObject();
    auto* targetObject = recipient->GetBaseObject();
    if (!supported(sourceObject) || !supported(targetObject)) return false;
    const auto* targetEnchant = recipient->extraList.GetByType<RE::ExtraEnchantment>();
    if (!targetEnchant || targetEnchant->enchantment != enchantment) {
        SKSE::log::warn("Transfer capture skipped: recipient does not hold the requested enchantment");
        return false;
    }
    const auto* donorExtra = donor->extraList.GetByType<RE::ExtraEnchantment>();
    auto* originalSource = sourceFor(sourceObject, &donor->extraList, enchantment, false);
    if (!originalSource && (!donorExtra || !donorExtra->enchantment) && baseEnchantment(sourceObject) == enchantment)
        originalSource = sourceObject;
    if (!originalSource && donorExtra && donorExtra->enchantment == enchantment)
        originalSource = sourceFor(sourceObject, &donor->extraList, enchantment, true);

    const auto text = description(originalSource);
    auto targetKey = keyFor(targetObject, &recipient->extraList);
    if (text.empty()) {
        std::lock_guard lock(mutex);
        bindings.entries.erase(targetKey);
        bindings.entries.erase(keyFor(sourceObject, &donor->extraList));
        SKSE::log::info("Transfer {:08X} -> {:08X}: use Skyrim's generated effect text (no custom donor description)",
            sourceObject->GetFormID(), targetObject->GetFormID());
        return true;
    }
    if (!targetKey.owner || !targetKey.unique) {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* inventory = player ? player->GetInventoryChanges() : nullptr;
        if (!inventory) return false;
        // Keep IDs already assigned by the engine/Wheeler. Allocate only when
        // absent; do not invent a private ID counter that can collide with it.
        std::uint16_t next = 0;
        for (unsigned attempt = 0; attempt < 65535; ++attempt) {
            const auto candidate = inventory->GetNextUniqueID();
            if (!candidate) continue;
            std::lock_guard lock(mutex);
            if (!bindings.entries.contains({targetObject->GetFormID(), player->GetFormID(), candidate})) {
                next = candidate;
                break;
            }
        }
        if (!next) return false;
        auto* uid = recipient->extraList.GetByType<RE::ExtraUniqueID>();
        if (uid) { uid->baseID = player->GetFormID(); uid->uniqueID = next; }
        else recipient->extraList.Add(new RE::ExtraUniqueID(player->GetFormID(), next));
        recipient->AddChange(RE::TESObjectREFR::ChangeFlags::kItemExtraData);
        targetKey = keyFor(targetObject, &recipient->extraList);
    }
    {
        std::lock_guard lock(mutex);
        // Transferring an already transferred enchantment keeps its first donor.
        bindings.entries.erase(keyFor(sourceObject, &donor->extraList));
        bindings.entries.insert_or_assign(targetKey, esd::Binding{enchantment->GetFormID(), originalSource->GetFormID()});
    }
    SKSE::log::info("Preserved description: source={:08X}, target={:08X}, enchantment={:08X}, owner={:08X}, uid={}",
        originalSource->GetFormID(), targetKey.object, enchantment->GetFormID(), targetKey.owner, targetKey.unique);
    return true;
}

void populate(RE::ItemCard* card, RE::InventoryEntryData* entry, bool inContainer) {
    originalPopulate(card, entry, inContainer);
    if (!ready || !card || !card->obj.IsObject() || !card->view || !entry || !supported(entry->object) || !entry->extraLists) return;
    RE::GFxValue oldText;
    const bool missingText = !card->obj.GetMember("effects", &oldText) || !oldText.IsString() || !*oldText.GetString();
    RE::TESBoundObject* donor = nullptr;
    RE::EnchantmentItem* enchantment = nullptr;
    for (auto* extras : *entry->extraLists) {
        const auto* extra = extras ? extras->GetByType<RE::ExtraEnchantment>() : nullptr;
        if (!extra || !extra->enchantment) continue;
        enchantment = extra->enchantment;
        donor = sourceFor(entry->object, extras, enchantment, missingText && !baseEnchantment(entry->object));
        if (donor) break;
    }
    const auto text = description(donor);
    if (text.empty()) return;
    // Store a Scaleform-managed string, not a pointer into a temporary std::string.
    RE::GFxValue value;
    card->view->CreateString(&value, text.c_str());
    card->infoText = text.c_str();
    card->obj.SetMember("effects", value);
    std::lock_guard lock(mutex);
    if (loggedCards.size() < 100 && loggedCards.emplace(entry->object->GetFormID(), enchantment->GetFormID()).second)
        SKSE::log::info("Item card restored: item={:08X}, enchantment={:08X}, source={:08X}",
            entry->object->GetFormID(), enchantment->GetFormID(), donor->GetFormID());
}

class UniqueChanges final : public RE::BSTEventSink<RE::TESUniqueIDChangeEvent> {
    RE::BSEventNotifyControl ProcessEvent(const RE::TESUniqueIDChangeEvent* e,
        RE::BSTEventSource<RE::TESUniqueIDChangeEvent>*) override {
        if (e) {
            std::lock_guard lock(mutex);
            bindings.move({e->objectID, e->oldBaseID, e->oldUniqueID}, {e->objectID, e->newBaseID, e->newUniqueID});
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} uniqueChanges;

void clear(SKSE::SerializationInterface*) {
    std::lock_guard lock(mutex);
    bindings.entries.clear();
    loggedCards.clear();
}
void save(SKSE::SerializationInterface* api) {
    std::lock_guard lock(mutex);
    for (const auto& [key, value] : bindings.entries) {
        const auto wire = esd::encode(key, value);
        if (!api->WriteRecord(kBindingRecord, 1, wire.data(), static_cast<std::uint32_t>(sizeof(wire)))) {
            SKSE::log::error("Could not save a description binding");
            break;
        }
    }
    SKSE::log::info("Saved {} description bindings", bindings.entries.size());
}
void load(SKSE::SerializationInterface* api) {
    clear(api);
    std::uint32_t type = 0, version = 0, length = 0;
    std::size_t loaded = 0;
    while (api->GetNextRecordInfo(type, version, length)) {
        if (type != kBindingRecord || version != 1 || length != sizeof(esd::Wire) || loaded >= 65535) continue;
        esd::Wire wire{};
        if (api->ReadRecordData(wire.data(), static_cast<std::uint32_t>(sizeof(wire))) != sizeof(wire)) continue;
        bool resolved = true;
        for (const auto index : {0, 1, 3, 4}) {
            RE::FormID id = 0;
            if (!api->ResolveFormID(wire[index], id)) { resolved = false; break; }
            wire[index] = id;
        }
        esd::Key key;
        esd::Binding value;
        if (!resolved || !esd::decode(wire, key, value)) continue;
        std::lock_guard lock(mutex);
        bindings.entries.insert_or_assign(key, value);
        ++loaded;
    }
    SKSE::log::info("Loaded {} description bindings", loaded);
}
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("RememberTransfer", "ESD_Native", remember);
    return true;
}
void message(SKSE::MessagingInterface::Message* event) {
    if (event->type != SKSE::MessagingInterface::kDataLoaded) return;
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data || !data->LookupModByName("EnchantmentSwapper.esp")) {
        SKSE::log::info("EnchantmentSwapper.esp is not loaded; helper inactive");
        return;
    }
    {
        std::lock_guard lock(mutex);
        catalog.clear();
        for (auto* weapon : data->GetFormArray<RE::TESObjectWEAP>())
            if (auto* enchantment = baseEnchantment(weapon)) catalog.add(enchantment->GetFormID(), true, weapon->GetFormID(), description(weapon));
        for (auto* armor : data->GetFormArray<RE::TESObjectARMO>())
            if (auto* enchantment = baseEnchantment(armor)) catalog.add(enchantment->GetFormID(), false, armor->GetFormID(), description(armor));
    }
    // ItemCard::PopulateInformation, ABI verified against KernalsEgg's headers.
    // Hook the function so all its item-menu callers use the same fix. Other
    // plugins' call-site hooks continue to call through this function normally.
    const auto address = REL::Relocation<std::uintptr_t>{REL::RelocationID(51019, 51897)}.address();
    const auto initialized = MH_Initialize();
    if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED) return;
    const auto created = MH_CreateHook(reinterpret_cast<void*>(address), reinterpret_cast<void*>(populate), reinterpret_cast<void**>(&originalPopulate));
    if (created != MH_OK) { SKSE::log::error("Item-card hook creation failed: {}", int(created)); return; }
    const auto enabled = MH_EnableHook(reinterpret_cast<void*>(address));
    if (enabled != MH_OK) { SKSE::log::error("Item-card hook enabling failed: {}", int(enabled)); return; }
    RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESUniqueIDChangeEvent>(&uniqueChanges);
    ready = true;
    SKSE::log::info("Ready: item-card description preservation; no polling or recurring Papyrus updates");
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData d{};
    d.PluginVersion({1,0,0,0}); d.PluginName("EnchantmentSwapperDescriptions");
    d.AuthorName("Physics-helper contributors"); d.UsesAddressLibrary(true); d.UsesStructsPost629(true);
    d.CompatibleVersions({REL::Version{1,6,1170,0}}); return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "EnchantmentSwapperDescriptions.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("EnchantmentSwapperDescriptions 1.0.0; Skyrim 1.6.1170");
    const auto* api = SKSE::GetSerializationInterface();
    api->SetUniqueID(kSerialization); api->SetSaveCallback(save); api->SetLoadCallback(load); api->SetRevertCallback(clear);
    return SKSE::GetPapyrusInterface()->Register(registerPapyrus) && SKSE::GetMessagingInterface()->RegisterListener(message);
}
