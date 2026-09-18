#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <memory>
#include <vector>
#include "Rules.h"
#include "Crafting.h"

namespace {
constexpr auto sourceFile = "RunemasterMagic.esl";
constexpr auto bridgeFile = "Runemaster Enchanted Weapons.esp";
struct Rune {
    RE::EffectSetting *self{}, *transfer{};
    RE::EnchantmentItem* enchantment{};
    RE::SpellItem *power{}, *bridge{};
    bool singleUse{};
    const char* name{};
};
std::array<Rune, 8> runeData;
struct PermanentRune {
    RE::TESObjectWEAP* weapon{};
    RE::EnchantmentItem* enchantment{};
    RE::SpellItem *bridge{}, *cooldown{};
    RE::BGSConstructibleObject* recipe{};
    const char* name{};
};
std::array<PermanentRune, 8> permanentData;
RE::EffectSetting* runicCooldown{};
RE::BGSKeyword* disallowEnchanting{};
std::atomic_bool ready = false, session = false;
std::atomic_uint64_t epoch = 0;
std::uint32_t diagnosticHits = 0;

bool active(RE::ActiveEffect* effect) {
    using Flag = RE::ActiveEffect::Flag;
    return effect && runes::active(effect->flags.any(Flag::kInactive),
        effect->flags.any(Flag::kDispelled), effect->elapsedSeconds, effect->duration);
}
Rune* identify(RE::EffectSetting* effect) {
    if (!effect) return nullptr;
    for (auto& rune : runeData)
        if (effect == rune.self || effect == rune.transfer) return &rune;
    return nullptr;
}
bool runeID(RE::FormID id) {
    for (const auto& rune : runeData)
        if ((rune.self && rune.self->GetFormID() == id) ||
            (rune.transfer && rune.transfer->GetFormID() == id)) return true;
    return false;
}
struct Selection { RE::ActiveEffect* effect{}; Rune* rune{}; };
Selection select(RE::Actor* actor) {
    Selection best;
    auto target = actor ? actor->GetMagicTarget() : nullptr;
    auto effects = target ? target->GetActiveEffectList() : nullptr;
    if (!effects) return best;
    for (auto effect : *effects) {
        if (!active(effect)) continue;
        auto rune = identify(effect->GetBaseObject());
        if (!rune) continue;
        if (!best.effect || runes::newer(effect->elapsedSeconds, effect->usUniqueID,
            best.effect->elapsedSeconds, best.effect->usUniqueID)) best = {effect, rune};
    }
    return best;
}
void expireOtherRunes(RE::Actor* actor, RE::ActiveEffect* keep) {
    auto target = actor ? actor->GetMagicTarget() : nullptr;
    auto effects = target ? target->GetActiveEffectList() : nullptr;
    if (!effects) return;
    // Never invalidate the list while iterating it.
    std::vector<RE::ActiveEffect*> old;
    for (auto effect : *effects)
        if (effect != keep && active(effect) && identify(effect->GetBaseObject())) old.push_back(effect);
    for (auto effect : old) effect->Dispel(true);
}
bool effectPresent(RE::Actor* actor, RE::EffectSetting* base) {
    auto target = actor ? actor->GetMagicTarget() : nullptr;
    auto effects = target ? target->GetActiveEffectList() : nullptr;
    if (!effects) return false;
    for (auto effect : *effects)
        if (active(effect) && effect->GetBaseObject() == base) return true;
    return false;
}
bool cannotRecast(RE::Actor* target, RE::SpellItem* spell) {
    for (auto effect : spell->effects)
        if (effect && effect->baseEffect &&
            effect->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoRecast) &&
            effectPresent(target, effect->baseEffect)) return true;
    return false;
}
void applyHit(RE::Actor* attacker, RE::Actor* victim, RE::FormID weaponID) {
    if (!attacker || !victim || attacker == victim) return;
    auto current = select(attacker);
    if (!current.rune) return;
    expireOtherRunes(attacker, current.effect);
    // The original impact effects use No Recast. This also filters duplicate
    // hit notifications emitted by enchanted weapons before Papyrus runs.
    if (cannotRecast(victim, current.rune->bridge)) return;
    auto caster = attacker->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
    if (!caster) return;
    auto& rune = *current.rune;
    // Mark one-shot runes consumed synchronously. Their original script still
    // handles the proc, delays, explosions and DispelSpell call normally.
    if (rune.singleUse) current.effect->Dispel(true);
    caster->CastSpellImmediate(rune.bridge, false, victim, 1.f, false, 0.f, attacker);
    if (diagnosticHits < 24) {
        ++diagnosticHits;
        SKSE::log::info("Rune hit: {}; attacker={:08X}; target={:08X}; weapon={:08X}; single-use={}",
            rune.name, attacker->GetFormID(), victim->GetFormID(), weaponID, rune.singleUse);
        if (diagnosticHits == 24) SKSE::log::info("Routine hit logging suppressed for the remainder of this session");
    }
}

void applyPermanentHit(RE::Actor* attacker, RE::Actor* victim, RE::FormID weaponID) {
    if (!attacker || !victim || attacker == victim) return;
    PermanentRune* found = nullptr;
    for (auto& rune : permanentData)
        if (rune.weapon && rune.weapon->GetFormID() == weaponID) { found = &rune; break; }
    if (!found) return;
    auto target = attacker->GetMagicTarget();
    if (!target || target->HasMagicEffect(runicCooldown) || cannotRecast(victim, found->bridge)) return;
    // Match the original impact conditions before applying its cooldown.
    // In particular, its Swap Subject/Target condition checks the wielder.
    for (auto effect : found->bridge->effects) {
        if (!effect->conditions.IsTrue(victim, attacker) ||
            !effect->baseEffect->conditions.IsTrue(victim, attacker)) return;
    }
    auto caster = attacker->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
    if (!caster) return;
    caster->CastSpellImmediate(found->bridge, false, victim, 1.f, false, 0.f, attacker);
    // Apply the SAME original cooldown spell immediately, closing the gap
    // before its Papyrus script runs. Its existing condition prevents a second
    // application from refreshing or stacking the cooldown later.
    caster->CastSpellImmediate(found->cooldown, true, attacker, 1.f, false, 0.f, attacker);
    if (diagnosticHits < 24) {
        ++diagnosticHits;
        SKSE::log::info("Permanent rune hit: {}; attacker={:08X}; target={:08X}; source weapon={:08X}; cooldown active={}",
            found->name, attacker->GetFormID(), victim->GetFormID(), weaponID,
            target->HasMagicEffect(runicCooldown));
    }
}

class Events final : public RE::BSTEventSink<RE::TESHitEvent>,
                     public RE::BSTEventSink<RE::TESMagicEffectApplyEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* event,
        RE::BSTEventSource<RE::TESHitEvent>*) override {
        if (!event || !ready || !session) return RE::BSEventNotifyControl::kContinue;
        auto attacker = event->cause ? event->cause->As<RE::Actor>() : nullptr;
        auto victim = event->target ? event->target->As<RE::Actor>() : nullptr;
        auto weapon = RE::TESForm::LookupByID<RE::TESObjectWEAP>(event->source);
        if (!runes::eligibleHit(attacker && victim && attacker != victim,
            weapon && !weapon->IsHandToHandMelee(),
            weapon && weapon->IsStaff(), static_cast<std::uint8_t>(event->flags.underlying())))
            return RE::BSEventNotifyControl::kContinue;
        const auto attackerHandle = attacker->GetHandle(), victimHandle = victim->GetHandle();
        const auto generation = epoch.load();
        const auto weaponID = event->source;
        SKSE::GetTaskInterface()->AddTask([attackerHandle, victimHandle, generation, weaponID] {
            if (!ready || !session || epoch != generation) return;
            auto attacker = attackerHandle.get(), victim = victimHandle.get();
            applyHit(attacker.get(), victim.get(), weaponID);
            applyPermanentHit(attacker.get(), victim.get(), weaponID);
        });
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* event,
        RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override {
        if (!event || !ready || !session || !runeID(event->magicEffect))
            return RE::BSEventNotifyControl::kContinue;
        auto actor = event->target ? event->target->As<RE::Actor>() : nullptr;
        if (!actor) return RE::BSEventNotifyControl::kContinue;
        const auto handle = actor->GetHandle();
        const auto generation = epoch.load();
        SKSE::GetTaskInterface()->AddTask([handle, generation] {
            if (!ready || !session || epoch != generation) return;
            auto actor = handle.get();
            if (!actor) return;
            auto current = select(actor.get());
            if (current.effect) expireOtherRunes(actor.get(), current.effect);
        });
        return RE::BSEventNotifyControl::kContinue;
    }
} events;

void copyConditions(RE::TESCondition& destination, const RE::TESCondition& source) {
    // Own separate condition nodes; the enchantment's list remains untouched.
    auto tail = &destination.head;
    for (auto item = source.head; item; item = item->next) {
        auto copy = new RE::TESConditionItem();
        copy->data = item->data;
        copy->next = nullptr;
        *tail = copy;
        tail = &copy->next;
    }
}
void copyImpactEffects(RE::EnchantmentItem* enchantment, RE::SpellItem* bridge) {
    for (auto effect : bridge->effects) delete effect;
    bridge->effects.clear();
    for (auto effect : enchantment->effects) {
        auto copy = new RE::Effect();
        copy->baseEffect = effect->baseEffect;
        copy->effectItem = effect->effectItem;
        copy->cost = effect->cost;
        copyConditions(copy->conditions, effect->conditions);
        bridge->effects.push_back(copy);
    }
    bridge->hostileCount = enchantment->hostileCount;
    bridge->avEffectSetting = enchantment->avEffectSetting;
}
bool resolve() {
    auto data = RE::TESDataHandler::GetSingleton();
    if (!data) return false;
    using Archetype = RE::EffectArchetypes::ArchetypeID;
    for (std::size_t i = 0; i < runeData.size(); ++i) {
        const auto& def = runes::definitions[i];
        auto& rune = runeData[i];
        rune.name = def.name;
        rune.singleUse = def.singleUse;
        rune.self = data->LookupForm<RE::EffectSetting>(def.self, sourceFile);
        rune.transfer = data->LookupForm<RE::EffectSetting>(def.transfer, sourceFile);
        rune.power = data->LookupForm<RE::SpellItem>(def.power, sourceFile);
        rune.bridge = data->LookupForm<RE::SpellItem>(def.bridge, bridgeFile);
        if (!rune.self || !rune.transfer || !rune.power || !rune.bridge) {
            SKSE::log::error("Missing required forms for {}; enable Runemaster Magic 1.5 and {}", def.name, bridgeFile);
            return false;
        }
        auto associated = rune.self->data.associatedForm;
        rune.enchantment = associated ? associated->As<RE::EnchantmentItem>() : nullptr;
        if (!rune.enchantment || rune.enchantment->effects.empty() ||
            rune.transfer->data.associatedForm != associated ||
            rune.self->data.archetype != Archetype::kEnhanceWeapon ||
            rune.transfer->data.archetype != Archetype::kEnhanceWeapon) {
            SKSE::log::error("Unexpected rune implementation for {}; no rune records were changed", def.name);
            return false;
        }
        for (auto effect : rune.enchantment->effects) {
            if (!effect || !effect->baseEffect ||
                effect->baseEffect->data.castingType != RE::MagicSystem::CastingType::kFireAndForget ||
                effect->baseEffect->data.delivery != RE::MagicSystem::Delivery::kTouch) {
                SKSE::log::error("Unsupported impact effect for {}; no rune records were changed", def.name);
                return false;
            }
        }
        // The original effects have zero magnitude: WeaponSpeedMult is only
        // EnhanceWeapon scaffolding. Do not silently drop a modded AV bonus.
        for (auto effect : rune.power->effects) {
            if (effect && (effect->baseEffect == rune.self || effect->baseEffect == rune.transfer) &&
                effect->effectItem.magnitude != 0.f) {
                SKSE::log::error("{} has a nonzero weapon-speed bonus in the winning record; refusing to remove it", def.name);
                return false;
            }
        }
    }
    return true;
}
bool resolvePermanent() {
    auto data = RE::TESDataHandler::GetSingleton();
    runicCooldown = data->LookupForm<RE::EffectSetting>(0x82F, sourceFile);
    disallowEnchanting = data->LookupForm<RE::BGSKeyword>(0xC27BD, "Skyrim.esm");
    if (!runicCooldown || !disallowEnchanting) return false;
    for (std::size_t i = 0; i < permanentData.size(); ++i) {
        const auto& def = runes::permanentDefinitions[i];
        auto& rune = permanentData[i];
        rune.name = def.name;
        rune.weapon = data->LookupForm<RE::TESObjectWEAP>(def.weapon, sourceFile);
        rune.enchantment = data->LookupForm<RE::EnchantmentItem>(def.enchantment, sourceFile);
        rune.bridge = data->LookupForm<RE::SpellItem>(def.bridge, bridgeFile);
        rune.cooldown = data->LookupForm<RE::SpellItem>(def.cooldown, sourceFile);
        rune.recipe = data->LookupForm<RE::BGSConstructibleObject>(def.recipe, sourceFile);
        if (!rune.weapon || !rune.enchantment || !rune.bridge || !rune.cooldown || rune.enchantment->effects.empty()) {
            SKSE::log::error("Missing permanent rune forms for {}; install the matching 1.1 ESP and DLL", def.name);
            return false;
        }
        for (auto effect : rune.enchantment->effects) {
            if (!effect || !effect->baseEffect ||
                effect->baseEffect->data.castingType != RE::MagicSystem::CastingType::kFireAndForget ||
                effect->baseEffect->data.delivery != RE::MagicSystem::Delivery::kTouch) {
                SKSE::log::error("Unsupported permanent rune effects on {}", def.name); return false;
            }
        }
        bool hasCooldown = false;
        for (auto effect : rune.cooldown->effects)
            if (effect && effect->baseEffect == runicCooldown) hasCooldown = true;
        if (!hasCooldown) { SKSE::log::error("Unexpected cooldown spell on {}", def.name); return false; }
        if (rune.recipe && rune.recipe->createdItem != rune.weapon) {
            SKSE::log::warn("Recipe for {} has a different output; crafting preservation disabled for this recipe", def.name);
            rune.recipe = nullptr;
        }
    }
    return true;
}
bool enabled() { return ready && session; }
void initialize() {
    auto holder = RE::ScriptEventSourceHolder::GetSingleton();
    if (!holder || !resolve() || !resolvePermanent()) return;
    // Resolve and validate everything before changing the source's live forms.
    // Keep winning stats, effect definitions, costs, perks and scripts.
    for (auto& rune : runeData) copyImpactEffects(rune.enchantment, rune.bridge);
    for (auto& rune : permanentData) copyImpactEffects(rune.enchantment, rune.bridge);
    for (auto& rune : runeData) {
        for (auto effect : {rune.self, rune.transfer}) {
            effect->data.archetype = RE::EffectArchetypes::ArchetypeID::kScript;
            effect->data.primaryAV = RE::ActorValue::kNone;
            effect->data.secondaryAV = RE::ActorValue::kNone;
            effect->data.associatedForm = nullptr;
        }
        SKSE::log::info("Prepared {}: self={:08X}; transfer={:08X}; impact={:08X}; single-use={}",
            rune.name, rune.self->GetFormID(), rune.transfer->GetFormID(), rune.bridge->GetFormID(), rune.singleUse);
    }
    std::array<RE::BGSConstructibleObject*, 8> recipes;
    for (std::size_t i = 0; i < permanentData.size(); ++i) {
        auto& rune = permanentData[i];
        // Only detach this weapon's original permanent rune enchantment.
        // A different base enchantment installed by another patch is retained.
        if (rune.weapon->formEnchanting == rune.enchantment) {
            rune.weapon->formEnchanting = nullptr;
            rune.weapon->amountofEnchantment = 0;
        }
        rune.weapon->RemoveKeyword(disallowEnchanting);
        recipes[i] = rune.recipe;
        SKSE::log::info("Permanent rune ready: {}; weapon={:08X}; normal base enchantment retained={}",
            rune.name, rune.weapon->GetFormID(), rune.weapon->formEnchanting != nullptr);
    }
    runes::crafting::install(recipes, enabled);
    holder->AddEventSink<RE::TESHitEvent>(&events);
    holder->AddEventSink<RE::TESMagicEffectApplyEvent>(&events);
    ready = true;
    SKSE::log::info("Ready: eight castable runes, eight Transfer Rune variants, eight enchantable permanent rune weapons");
}
void newSession(bool successful) {
    session = false;
    runes::crafting::reset();
    ++epoch;
    diagnosticHits = 0;
    session = successful;
}
void message(SKSE::MessagingInterface::Message* message) {
    switch (message->type) {
    case SKSE::MessagingInterface::kDataLoaded: initialize(); break;
    case SKSE::MessagingInterface::kPreLoadGame: newSession(false); break;
    case SKSE::MessagingInterface::kNewGame: newSession(true); break;
    case SKSE::MessagingInterface::kPostLoadGame: newSession(message->data != nullptr); break;
    default: break;
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({1,1,0,0}); data.PluginName("RunemasterEnchantmentBridge");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}}); return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory(); if (!path) return false;
    *path /= "RunemasterEnchantmentBridge.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("Runemaster Enchanted Weapons 1.1.0; Skyrim 1.6.1170; original Runemaster Magic 1.5 required");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
