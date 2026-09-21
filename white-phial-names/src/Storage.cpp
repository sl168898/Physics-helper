#include "Storage.h"
#include "Bank.h"
#include "NamedKeywords.h"
#include "CosaveFile.h"
#include "REX/W32/OLE32.h"
#include "REX/W32/SHELL32.h"
#include <map>
#include <mutex>

namespace phial::storage {
namespace {
constexpr auto mod = "White Phial - Decanting.esp";
std::recursive_mutex gate;
Bank bank, previousBank;
Bytes preloaded;
std::array<RE::AlchemyItem*, slotCount> slots{};
RE::TESGlobal* hashLow{};
RE::TESGlobal* hashHigh{};
bool initialized{}, ready{}, loading{}, callbackSeen{}, preloadOK{};
bool warned{};
std::string fault;
// Engine ActiveEffect objects borrow Effect*. Keep previously published effect
// arrays alive through save switches. Memoize by definition, so loading the
// same save repeatedly does not allocate again. These game-heap allocations
// live until process exit; never delete a borrowed effect during teardown.
std::map<Bytes, std::vector<RE::Effect*>> effectCache;

void error(std::string message) {
    fault = std::move(message); ready = false;
    SKSE::log::error("Protected bottles: {}", fault);
}
std::string str(const char* p) { return p ? p : ""; }
std::string describe(RE::TESForm* form) {
    if (!form) return "<none>";
    auto* file = form->GetFile(0);
    return fmt::format("{:08X} type={} editorID=\"{}\" name=\"{}\" source=\"{}\"",
        form->GetFormID(), RE::FormTypeToString(form->GetFormType()),
        str(form->GetFormEditorID()), str(form->GetName()),
        file ? std::string(file->GetFilename()) : "<no plugin>");
}
Ref reference(RE::TESForm* form, std::string_view field) {
    if (!form) return {};
    if ((form->GetFormID() >> 24) == 0xFF)
        throw Error(fmt::format("Temporary dependency at {}: {}", field, describe(form)));
    auto* file = form->GetFile(0);
    if (!file) throw Error(fmt::format("Dependency has no plugin at {}: {}", field, describe(form)));
    return {std::string(file->GetFilename()), form->GetFormID() & (file->IsLight() ? 0xFFFu : 0xFFFFFFu)};
}
void logDependencies(RE::AlchemyItem* potion) {
    // One snapshot on failure, not a recurring scan. Names and IDs are copied
    // to the log; no borrowed form/inventory pointers are retained.
    SKSE::log::info("Rejected liquid: {}", describe(potion));
    const auto log = [](std::string_view field, RE::TESForm* form) {
        SKSE::log::info("Liquid dependency {}: {}", field, describe(form));
    };
    log("equipSlot", potion->equipSlot);
    log("addictionItem", potion->data.addictionItem);
    log("consumptionSound", potion->data.consumptionSound);
    log("pickupSound", potion->pickupSound);
    log("putdownSound", potion->putdownSound);
    std::size_t index = 0;
    for (auto* keyword : potion->GetKeywords()) {
        if (index >= 256) break;
        log(fmt::format("keywords[{}]", index++), keyword);
    }
    index = 0;
    for (auto* effect : potion->effects) {
        if (index >= 128) break;
        log(fmt::format("effects[{}].base", index++), effect ? effect->baseEffect : nullptr);
    }
}
template<class T> T* resolve(const Ref& r) {
    if (r.file.empty()) return nullptr;
    auto* f = RE::TESDataHandler::GetSingleton()->LookupForm<T>(r.local, r.file);
    if (!f) throw Error("Missing or changed liquid dependency: " + r.file);
    return f;
}
std::optional<std::size_t> slotIndex(RE::AlchemyItem* p) {
    auto it = std::find(slots.begin(), slots.end(), p);
    if (!p || it == slots.end()) return {};
    return static_cast<std::size_t>(it - slots.begin());
}
NamedKeywords<RE::BGSKeyword> keywordIndex() {
    NamedKeywords<RE::BGSKeyword> index;
    auto* data = RE::TESDataHandler::GetSingleton();
    if (!data) throw Error("Keyword data is not available");
    for (auto* keyword : data->GetFormArray<RE::BGSKeyword>())
        if (keyword) index.add(str(keyword->GetFormEditorID()), keyword);
    return index;
}
Liquid snapshot(RE::AlchemyItem* p, const std::string& name, const NamedKeywords<RE::BGSKeyword>& keywords) {
    // Crafted potions have unconditioned effect lists. Do not silently strip
    // unsupported model/destruction data or conditional effects from a mod.
    if (!p || p->numAlternateTextures || p->numAddons || p->RE::BGSDestructibleObjectForm::data ||
        p->effects.empty() || p->effects.size() > 128 || p->numKeywords > 256)
        throw Error("This custom liquid has unsupported data; sample was not consumed");
    Liquid l;
    l.name = name.empty() ? str(p->GetName()) : name;
    l.model = str(p->GetModel());
    l.icon = str(static_cast<RE::TESIcon*>(p)->textureName.c_str());
    l.messageIcon = str(p->RE::BGSMessageIcon::icon.textureName.c_str());
    l.secondaryIcon = str(p->messageIcon.textureName.c_str());
    l.weight = p->weight; l.addictionChance = p->data.addictionChance;
    l.value = p->data.costOverride; l.flags = p->data.flags.underlying();
    l.medicineRecord = (p->formFlags & (1u << 29)) != 0;
    l.bounds = {p->boundData.boundMin.x, p->boundData.boundMin.y, p->boundData.boundMin.z,
        p->boundData.boundMax.x, p->boundData.boundMax.y, p->boundData.boundMax.z};
    l.equip = reference(p->equipSlot, "equipSlot"); l.addiction = reference(p->data.addictionItem, "addictionItem");
    l.consumeSound = reference(p->data.consumptionSound, "consumptionSound");
    l.pickupSound = reference(p->pickupSound, "pickupSound"); l.putdownSound = reference(p->putdownSound, "putdownSound");
    for (auto* k : p->GetKeywords()) {
        if (!k) throw Error("Null liquid keyword");
        if ((k->GetFormID() >> 24) == 0xFF) {
            const auto editorID = str(k->GetFormEditorID());
            if (keywords.resolve(editorID) != k) throw Error("Runtime keyword identity mismatch: " + editorID);
            l.namedKeywords.push_back(editorID);
            SKSE::log::info("Preserving runtime keyword by EditorID: {} (current ID {:08X})", editorID, k->GetFormID());
        } else l.keywords.push_back(reference(k, fmt::format("keywords[{}]", l.keywords.size())));
    }
    for (auto* e : p->effects) {
        if (!e || !e->baseEffect || e->conditions.head) throw Error("This liquid has missing or conditional effects");
        l.effects.push_back({reference(e->baseEffect, fmt::format("effects[{}].base", l.effects.size())),
            e->effectItem.magnitude, e->cost, e->effectItem.area, e->effectItem.duration});
    }
    Writer check; check.liquid(l); return l;
}
struct Prepared {
    const Liquid* source{};
    std::vector<RE::Effect*> effects;
    std::vector<RE::BGSKeyword*> keywords;
    RE::BGSEquipSlot* equip{};
    RE::SpellItem* addiction{};
    RE::BGSSoundDescriptorForm *consume{}, *pickup{}, *putdown{};
};
Prepared prepare(const Liquid& l, const NamedKeywords<RE::BGSKeyword>& keywords) {
    Prepared p; p.source = &l;
    p.equip = resolve<RE::BGSEquipSlot>(l.equip);
    p.addiction = resolve<RE::SpellItem>(l.addiction);
    p.consume = resolve<RE::BGSSoundDescriptorForm>(l.consumeSound);
    p.pickup = resolve<RE::BGSSoundDescriptorForm>(l.pickupSound);
    p.putdown = resolve<RE::BGSSoundDescriptorForm>(l.putdownSound);
    for (auto& k : l.keywords) p.keywords.push_back(resolve<RE::BGSKeyword>(k));
    for (auto& name : l.namedKeywords) p.keywords.push_back(keywords.resolve(name));
    std::vector<RE::EffectSetting*> bases;
    for (auto& e : l.effects) {
        auto* base = resolve<RE::EffectSetting>(e.base);
        if (!base) throw Error("Missing effect dependency");
        bases.push_back(base);
    }
    Writer w; w.liquid(l);
    if (auto it = effectCache.find(w.data); it != effectCache.end()) p.effects = it->second;
    else {
        for (std::size_t i = 0; i < l.effects.size(); ++i) {
            auto* e = new RE::Effect();
            e->baseEffect = bases[i]; e->effectItem.magnitude = l.effects[i].magnitude;
            e->effectItem.area = l.effects[i].area; e->effectItem.duration = l.effects[i].duration;
            e->cost = l.effects[i].cost; p.effects.push_back(e);
        }
        effectCache.emplace(std::move(w.data), p.effects);
    }
    return p;
}
void publish(RE::AlchemyItem* p, const Prepared& prepared) {
    const auto& l = *prepared.source;
    p->effects.clear();
    for (auto* e : prepared.effects) p->effects.push_back(e);
    p->hostileCount = 0;
    for (auto* e : prepared.effects) if (e->baseEffect->IsHostile()) ++p->hostileCount;
    p->avEffectSetting = nullptr; // Lazily recomputed by MagicItem::GetAVEffect.
    p->fullName = l.name;
    p->SetModel(l.model.c_str());
    static_cast<RE::TESIcon*>(p)->textureName = l.icon;
    p->RE::BGSMessageIcon::icon.textureName = l.messageIcon;
    p->messageIcon.textureName = l.secondaryIcon;
    p->weight = l.weight;
    p->data.costOverride = l.value; p->data.flags = static_cast<RE::AlchemyItem::AlchemyFlag>(l.flags);
    p->data.addictionChance = l.addictionChance; p->data.addictionItem = prepared.addiction;
    p->data.consumptionSound = prepared.consume;
    p->equipSlot = prepared.equip; p->pickupSound = prepared.pickup; p->putdownSound = prepared.putdown;
    if (l.medicineRecord) p->formFlags |= 1u << 29; else p->formFlags &= ~(1u << 29);
    p->boundData.boundMin = {l.bounds[0], l.bounds[1], l.bounds[2]};
    p->boundData.boundMax = {l.bounds[3], l.bounds[4], l.bounds[5]};
    std::vector<RE::BGSKeyword*> old(p->GetKeywords().begin(), p->GetKeywords().end());
    if (!old.empty()) p->RemoveKeywords(old);
    if (!prepared.keywords.empty()) p->AddKeywords(prepared.keywords);
}
void clearSlots() {
    for (auto* p : slots) if (p) {
        p->effects.clear(); p->hostileCount = 0; p->avEffectSetting = nullptr;
        p->fullName = "Protected liquid (definition unavailable)";
        p->data.flags = RE::AlchemyItem::AlchemyFlag::kMedicine;
        p->data.costOverride = 0;
    }
}
void apply(const Bank& b) {
    // Resolve every dependency before changing a single published record.
    std::vector<Prepared> prepared; prepared.reserve(b.liquids.size());
    const auto keywords = keywordIndex();
    for (auto& l : b.liquids) prepared.push_back(prepare(l, keywords));
    clearSlots();
    for (std::size_t i = 0; i < prepared.size(); ++i) publish(slots[i], prepared[i]);
}
std::uint32_t savedFingerprint() {
    auto read = [](RE::TESGlobal* g) -> std::uint32_t {
        if (!g || !std::isfinite(g->value) || g->value < 0 || g->value > 65535 || std::floor(g->value) != g->value)
            throw Error("Invalid saved liquid fingerprint");
        return static_cast<std::uint32_t>(g->value);
    };
    return read(hashLow) | (read(hashHigh) << 16);
}
void stamp() {
    // Mutable globals are saved in Skyrim's Global Variables global-data table.
    // Two 16-bit halves avoid rounding a 32-bit checksum through a float.
    const auto h = fingerprint(bank);
    hashLow->value = static_cast<float>(h & 65535); hashHigh->value = static_cast<float>(h >> 16);
}
bool isReady(RE::StaticFunctionTag*) {
    std::lock_guard lock(gate);
    if (!initialized || !ready || loading) return false;
    try { return savedFingerprint() == fingerprint(bank); }
    catch (const Error& e) { error(e.what()); return false; }
}
RE::AlchemyItem* protect(RE::StaticFunctionTag*, RE::AlchemyItem* potion, RE::BSFixedString chosenName) {
    std::lock_guard lock(gate);
    if (!isReady(nullptr) || !potion) return nullptr;
    SKSE::log::info("Protection request: potion={:08X}, chosenName=\"{}\"", potion->GetFormID(), chosenName.c_str());
    try {
        auto i = slotIndex(potion);
        if (i && *i >= bank.liquids.size()) throw Error("This protected liquid has no saved definition");
        if (i && (chosenName.empty() || bank.liquids[*i].name == chosenName.c_str())) return potion;
        if (!i && (potion->GetFormID() >> 24) != 0xFF) {
            const auto ref = reference(potion, "liquid");
            if (resolve<RE::AlchemyItem>(ref) != potion) throw Error("Liquid is not backed by a loaded plugin record");
            return potion;
        }
        const auto keywords = keywordIndex();
        Liquid liquid = i ? bank.liquids[*i] : snapshot(potion, chosenName.c_str(), keywords);
        if (i) liquid.name = chosenName.c_str();
        if (auto existing = bank.find(liquid)) return slots[*existing];
        if (bank.liquids.size() >= slotCount) throw Error("All 1792 protected liquid slots are occupied");
        // Preparation/validation occurs before committing the recipe or touching inventory.
        auto prepared = prepare(liquid, keywords);
        Bank next = bank; const auto index = next.append(liquid); (void)encode(next);
        publish(slots[index], prepared);
        bank = std::move(next); stamp();
        SKSE::log::info("Protected liquid {:08X} -> {:08X}; slot {}/{}; {}",
            potion->GetFormID(), slots[index]->GetFormID(), index + 1, slotCount, liquid.name);
        return slots[index];
    } catch (const std::exception& e) {
        SKSE::log::error("Cannot protect liquid: {}", e.what());
        logDependencies(potion);
        RE::DebugNotification("White Phial: liquid protection failed. Details in WhitePhialNames.log.");
        return nullptr;
    }
}
void traceNative(RE::StaticFunctionTag*, RE::BSFixedString text) {
    SKSE::log::info("Decant script: {}", text.c_str());
}
std::optional<Bytes> readCosave(std::string_view name) {
    wchar_t* buffer{};
    if (REX::W32::SHGetKnownFolderPath(REX::W32::FOLDERID_Documents,
        REX::W32::KF_FLAG_DEFAULT, nullptr, &buffer) != 0 || !buffer)
        throw Error("Cannot locate Windows Documents folder for Skyrim saves");
    const std::unique_ptr<wchar_t, decltype(&REX::W32::CoTaskMemFree)> documents(buffer, REX::W32::CoTaskMemFree);
    auto* setting = RE::GetINISetting("sLocalSavePath:General");
    const char* local = setting && setting->GetType() == RE::Setting::Type::kString ? setting->GetString() : "Saves";
    if (!local) throw Error("Skyrim sLocalSavePath is null");
    const auto path = cosavePath(documents.get(), local, name);
    SKSE::log::info("Preload save name=\"{}\"; normalized=\"{}\"; co-save=\"{}\"",
        name, cosaveFilename(name), path.string());
    auto bytes = readCosaveFile(path);
    if (bytes) SKSE::log::info("Read {} co-save bytes before engine save loading", bytes->size());
    else SKSE::log::warn("Selected co-save does not exist; continuing only if this save has no protected bank");
    return bytes;
}
void preLoad(const std::string& name) {
    previousBank = bank; ready = false; loading = true; callbackSeen = false;
    preloadOK = false; warned = false; fault.clear(); preloaded.clear(); bank = {};
    try {
        const auto bytes = readCosave(name);
        if (bytes) {
            if (auto record = bankFromCosave(*bytes)) { preloaded = *record; bank = decode(preloaded); }
            else SKSE::log::info("Selected co-save has no protected liquid record (legacy save)");
        }
        apply(bank); preloadOK = true;
        SKSE::log::info("Preloaded {} protected liquid definitions before engine save loading; fingerprint={:08X}",
            bank.liquids.size(), fingerprint(bank));
    } catch (const std::exception& e) { clearSlots(); bank = {}; error(e.what()); }
}
void postLoad(bool success) {
    loading = false;
    if (!success) {
        bank = std::move(previousBank);
        try { apply(bank); ready = initialized; } catch (const std::exception& e) { error(e.what()); }
        return;
    }
    previousBank = {};
    try {
        if (!fault.empty()) throw Error(fault);
        if (!preloadOK || (!preloaded.empty() && !callbackSeen)) throw Error("Protected liquid co-save could not be validated");
        if (savedFingerprint() != fingerprint(bank)) throw Error("The save and protected-liquid co-save do not match");
        ready = true;
        SKSE::log::info("Protected liquid bank ready; {} immutable definitions", bank.liquids.size());
    } catch (const std::exception& e) { error(e.what()); }
    if (!ready && !warned) {
        warned = true;
        RE::DebugMessageBox("White Phial could not restore its protected liquid definitions. Decanting and reassignment are disabled. Exit without overwriting this save. Keep its .ess and .skse files together. WhitePhialNames.log records the selected path and error.");
    }
}
}
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("IsReady", "WPD_Storage", isReady);
    vm->RegisterFunction("ProtectNative", "WPD_Storage", protect);
    vm->RegisterFunction("TraceNative", "WPD_Storage", traceNative);
    return true;
}
std::optional<Liquid> protectedDefinition(RE::AlchemyItem* potion) {
    std::lock_guard lock(gate);
    auto i = slotIndex(potion);
    if (!i) return {};
    // Snapshot already validated bank state; do not re-encode/hash the entire
    // bank once per catalog row. Actual duplication still calls IsReady.
    if (!initialized || !ready || loading || *i >= bank.liquids.size()) throw Error("Protected liquid is not ready");
    return bank.liquids[*i];
}
void revert() {
    std::lock_guard lock(gate);
    // SKSE's revert callback runs inside the load that PreLoad prepared.
    // Clearing here would invalidate the active effects being reconstructed.
    if (loading) return;
    ready = false; bank = {}; preloaded.clear(); fault.clear(); clearSlots();
}
void save(SKSE::SerializationInterface* api) {
    std::lock_guard lock(gate);
    if (!initialized || !ready || loading) return;
    try {
        if (savedFingerprint() != fingerprint(bank)) throw Error("Refusing to save mismatched liquid bank");
        auto bytes = encode(bank);
        if (!api->WriteRecord(bankRecord, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
            throw Error("SKSE could not write protected liquid definitions");
    } catch (const std::exception& e) { error(e.what()); RE::DebugNotification("White Phial: protected liquid save failed. Keep your previous save."); }
}
void loadRecord(SKSE::SerializationInterface* api, std::uint32_t version, std::uint32_t length) {
    std::lock_guard lock(gate);
    if (callbackSeen || version != 1 || length > maxBankBytes || length < 12) { error("Invalid protected liquid record"); return; }
    callbackSeen = true;
    Bytes bytes(length);
    if (api->ReadRecordData(bytes.data(), length) != length) error("Cannot read protected liquid record from SKSE");
    else if (bytes != preloaded) {
        SKSE::log::error("Liquid record comparison: preload={} bytes, SKSE={} bytes", preloaded.size(), bytes.size());
        if (fault.empty()) error(preloaded.empty() ?
            "SKSE found a protected liquid record that the early loader did not find; see the selected preload path" :
            "Preloaded and SKSE-loaded liquid records differ");
    }
    else SKSE::log::info("SKSE verified the preloaded protected liquid record ({} bytes)", bytes.size());
    // Do not replace Effect* during this callback: the engine may already have
    // restored active effects from the preloaded definitions.
}
void message(SKSE::MessagingInterface::Message* event) {
    std::lock_guard lock(gate);
    switch (event->type) {
    case SKSE::MessagingInterface::kDataLoaded: {
        auto* data = RE::TESDataHandler::GetSingleton();
        if (!data) return;
        for (std::size_t i = 0; i < slotCount; ++i) {
            slots[i] = data->LookupForm<RE::AlchemyItem>(firstSlot + static_cast<std::uint32_t>(i), mod);
            if (!slots[i]) { error("Updated White Phial - Decanting.esp is missing"); return; }
        }
        hashLow = data->LookupForm<RE::TESGlobal>(0x804, mod);
        hashHigh = data->LookupForm<RE::TESGlobal>(0x805, mod);
        initialized = hashLow && hashHigh;
        ready = false;
        SKSE::log::info("Protected bottle pool: {} slots; initialized={}", slotCount, initialized);
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
        if (initialized && event->data) preLoad(std::string(static_cast<const char*>(event->data), event->dataLen));
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
        if (initialized) postLoad(event->data != nullptr);
        break;
    case SKSE::MessagingInterface::kNewGame:
        if (initialized) { loading = false; bank = {}; preloaded.clear(); clearSlots(); stamp(); ready = true; fault.clear(); }
        break;
    default: break;
    }
}
}
