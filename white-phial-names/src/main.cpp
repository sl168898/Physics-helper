#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <mutex>
#include "Names.h"

namespace {
constexpr auto originalPlugin = "The White Phial - Tweaks and Enhancements.esp";
constexpr std::uint32_t saveID = 0x57504E31; // WPN1
constexpr std::uint32_t recordID = 0x4E414D45; // NAME
std::mutex mutex;
phial::Name contentsName;
RE::TESGlobal* assigning = nullptr;
const RE::TESFile* phialFile = nullptr;
RE::TESQuest* phialQuest = nullptr;
using Remove = RE::ObjectRefHandle (*)(RE::PlayerCharacter*, RE::TESBoundObject*, std::int32_t,
    RE::ITEM_REMOVE_REASON, RE::ExtraDataList*, RE::TESObjectREFR*, const RE::NiPoint3*, const RE::NiPoint3*);
REL::Relocation<Remove> originalRemove;

bool phialDestination(RE::TESObjectREFR* destination) {
    if (!destination || destination == RE::PlayerCharacter::GetSingleton()) return false;
    if (assigning && assigning->value != 0.0f) return true;
    // Some assignment scripts set the busy flag only after the gift arrives.
    // Also recognize their own target reference/base or service-quest alias.
    if (phialFile && (destination->GetFile(0) == phialFile ||
        (destination->GetBaseObject() && destination->GetBaseObject()->GetFile(0) == phialFile))) return true;
    if (phialQuest) for (auto* alias : phialQuest->aliases) {
        auto* referenceAlias = alias ? skyrim_cast<RE::BGSRefAlias*>(alias) : nullptr;
        if (referenceAlias && referenceAlias->GetReference() == destination) return true;
    }
    return false;
}

RE::ObjectRefHandle removeItem(RE::PlayerCharacter* player, RE::TESBoundObject* item, std::int32_t count,
    RE::ITEM_REMOVE_REASON reason, RE::ExtraDataList* extras, RE::TESObjectREFR* destination,
    const RE::NiPoint3* drop, const RE::NiPoint3* rotation) {
    // Capture before the gift consumes the last renamed bottle. The borrowed
    // inventory pointers are never retained or inspected after the engine call.
    phial::Name captured;
    bool haveSelection = false;
    if (player && item && count > 0 && destination && item->Is(RE::FormType::AlchemyItem)) {
        if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::GiftMenu::MENU_NAME)) {
            auto menu = ui->GetMenu<RE::GiftMenu>();
            auto* list = menu ? menu->GetRuntimeData().itemList : nullptr;
            auto* selected = list ? list->GetSelectedItem() : nullptr;
            auto* entry = selected ? selected->data.objDesc : nullptr;
            if (entry && entry->object == item && phialDestination(destination)) {
                // An explicit extra list is authoritative for split batches.
                const auto* name = extras ? extras->GetDisplayName(item) : entry->GetDisplayName();
                const auto* base = item->GetName();
                captured.assign(item->GetFormID(), name ? name : "", base ? base : "");
                haveSelection = true;
            }
        }
    }
    const auto result = originalRemove(player, item, count, reason, extras, destination, drop, rotation);
    if (haveSelection) {
        std::lock_guard lock(mutex);
        contentsName = std::move(captured);
        SKSE::log::info("Selected phial liquid: potion={:08X}, name={}", contentsName.potion,
            contentsName.text.empty() ? "<ordinary name>" : contentsName.text);
    }
    return result;
}

RE::BSFixedString getBottleName(RE::StaticFunctionTag*, RE::AlchemyItem* potion) {
    if (!potion) return {};
    std::lock_guard lock(mutex);
    const auto name = contentsName.forPotion(potion->GetFormID());
    if (!name.empty()) SKSE::log::info("Naming decanted dose: potion={:08X}, name={}", potion->GetFormID(), name);
    return RE::BSFixedString(name);
}
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("GetBottleName", "WPD_Names", getBottleName);
    return true;
}
void revert(SKSE::SerializationInterface*) {
    std::lock_guard lock(mutex);
    contentsName = {};
}
void save(SKSE::SerializationInterface* api) {
    std::lock_guard lock(mutex);
    if (!contentsName.potion || !api->OpenRecord(recordID, 1)) return;
    const auto length = static_cast<std::uint32_t>(contentsName.text.size());
    if (!api->WriteRecordData(&contentsName.potion, sizeof(contentsName.potion)) ||
        !api->WriteRecordData(&length, sizeof(length)) ||
        (length && !api->WriteRecordData(contentsName.text.data(), length)))
        SKSE::log::error("Could not save phial liquid name");
}
void load(SKSE::SerializationInterface* api) {
    revert(api);
    std::uint32_t type{}, version{}, length{};
    while (api->GetNextRecordInfo(type, version, length)) {
        if (type != recordID || version != 1) continue;
        std::uint32_t potion{}, size{};
        if (length < 8 || api->ReadRecordData(&potion, 4) != 4 || api->ReadRecordData(&size, 4) != 4 ||
            size > phial::maxNameBytes || size != length - 8) {
            SKSE::log::warn("Invalid phial name record; skipped");
            continue;
        }
        std::string text(size, '\0');
        if (size && api->ReadRecordData(text.data(), size) != size) continue;
        std::lock_guard lock(mutex);
        const bool restored = contentsName.restore(potion, std::move(text), [&](std::uint32_t oldID) {
            RE::FormID resolved{};
            return api->ResolveFormID(oldID, resolved) ? resolved : 0;
        });
        SKSE::log::info("Loaded phial name: resolved={}, potion={:08X}", restored, contentsName.potion);
    }
}
void message(SKSE::MessagingInterface::Message* event) {
    if (event->type != SKSE::MessagingInterface::kDataLoaded) return;
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data || !data->LookupForm<RE::TESQuest>(0x802, "White Phial - Decanting.esp")) return;
    assigning = data->LookupForm<RE::TESGlobal>(0x806, originalPlugin);
    phialFile = data->LookupModByName(originalPlugin);
    phialQuest = RE::TESForm::LookupByID<RE::TESQuest>(0x1010AA);
    if (!assigning) { SKSE::log::error("White Phial assignment global missing; name capture disabled"); return; }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
    originalRemove = table.write_vfunc(0x56, removeItem);
    SKSE::log::info("Ready: capture selected GiftMenu bottle names during phial assignment; no polling");
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData d{};
    d.PluginVersion({1,0,0,0}); d.PluginName("WhitePhialNames"); d.AuthorName("Physics-helper contributors");
    d.UsesAddressLibrary(true); d.UsesStructsPost629(true);
    d.CompatibleVersions({REL::Version{1,6,1170,0}}); return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "WhitePhialNames.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("WhitePhialNames 1.0.0; Decanting 1.2; Skyrim 1.6.1170");
    const auto* api = SKSE::GetSerializationInterface();
    api->SetUniqueID(saveID); api->SetSaveCallback(save); api->SetLoadCallback(load); api->SetRevertCallback(revert);
    return SKSE::GetPapyrusInterface()->Register(registerPapyrus) && SKSE::GetMessagingInterface()->RegisterListener(message);
}
