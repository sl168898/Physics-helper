#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <MinHook.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include "Rules.h"
#include "LabVisit.h"

namespace {
constexpr auto pluginFile = "Biggie Traits - Combined.esp";
RE::SpellItem *burden = nullptr, *guard = nullptr, *echo = nullptr;
RE::BGSKeyword* blessing = nullptr;
RE::SpellItem *labTrait = nullptr, *labBonus = nullptr;
RE::EffectSetting* labBonusEffect = nullptr;
traits::LabVisit labVisit;
std::atomic_uint64_t labEpoch = 0;
bool hadLabTrait = false;
std::atomic_bool ready = false, session = false;
std::recursive_mutex stateMutex;
traits::Combat combat;
RE::ATTACK_STATE_ENUM lastState = RE::ATTACK_STATE_ENUM::kNone;
RE::BGSAttackData* lastData = nullptr;
using HitFunction = void (*)(RE::Actor*, RE::HitData&);
HitFunction originalHit = nullptr;
REL::Relocation<void (*)(RE::PlayerCharacter*, float)> originalUpdate;

bool selected(RE::SpellItem* spell) {
    auto p = RE::PlayerCharacter::GetSingleton();
    return ready && session && p && spell && p->HasSpell(spell);
}
bool selectedLab() {
    auto p = RE::PlayerCharacter::GetSingleton();
    return session && p && labTrait && labBonus && labBonusEffect && p->HasSpell(labTrait);
}
void reset() {
    std::lock_guard lock(stateMutex); combat = {}; lastData = nullptr; lastState = RE::ATTACK_STATE_ENUM::kNone;
    labVisit.cancel(); ++labEpoch; hadLabTrait = false;
}
bool alchemyFurniture(RE::TESObjectREFR* ref) {
    auto base = ref ? ref->GetBaseObject() : nullptr;
    auto furniture = base ? base->As<RE::TESFurniture>() : nullptr;
    using Bench = RE::TESFurniture::WorkBenchData::BenchType;
    return furniture && (furniture->workBenchData.benchType == Bench::kAlchemy ||
        furniture->workBenchData.benchType == Bench::kAlchemyExperiment);
}
bool alchemyMenu() {
    auto ui = RE::UI::GetSingleton();
    auto menu = ui ? ui->GetMenu<RE::CraftingMenu>() : nullptr;
    auto sub = menu ? menu->GetCraftingSubMenu() : nullptr;
    // The pinned CommonLib header nests this class in a second CraftingSubMenus namespace.
    using AlchemyMenu = RE::CraftingSubMenus::CraftingSubMenus::AlchemyMenu;
    return sub && skyrim_cast<AlchemyMenu*>(sub);
}
void dispelLab(RE::PlayerCharacter* p) {
    if (!p || !labBonus) return;
    auto handle = p->GetHandle();
    p->GetMagicTarget()->DispelEffect(labBonus, handle);
}
void finishLab(const char* reason) {
    if (!labVisit.finish()) return;
    const auto epoch = labEpoch.load();
    SKSE::GetTaskInterface()->AddTask([epoch, reason] {
        std::lock_guard lock(stateMutex);
        auto p = RE::PlayerCharacter::GetSingleton();
        if (epoch != labEpoch || !selectedLab() || p->IsDead()) return;
        auto caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster) { SKSE::log::error("Lab Skeever: instant caster unavailable"); return; }
        dispelLab(p);
        caster->CastSpellImmediate(labBonus, true, p, 1.f, false, 0.f, p);
        SKSE::log::info("Lab Skeever: {} -> 20-second bonus cast; active={}", reason,
            p->GetMagicTarget()->HasMagicEffect(labBonusEffect));
    });
}
class LabEvents final : public RE::BSTEventSink<RE::TESFurnitureEvent>, public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::TESFurnitureEvent* e, RE::BSTEventSource<RE::TESFurnitureEvent>*) override {
        if (!e || !selectedLab() || e->actor.get() != RE::PlayerCharacter::GetSingleton()) return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(stateMutex);
        if (e->type == RE::TESFurnitureEvent::FurnitureEventType::kEnter) {
            labVisit.enter(alchemyFurniture(e->targetFurniture.get()));
            if (labVisit.armed) SKSE::log::info("Lab Skeever: entered alchemy workbench");
        } else if (e->type == RE::TESFurnitureEvent::FurnitureEventType::kExit) {
            finishLab("workbench exit");
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if (!e || e->menuName != RE::CraftingMenu::MENU_NAME || !selectedLab()) return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(stateMutex);
        if (e->opening) {
            auto p = RE::PlayerCharacter::GetSingleton();
            auto ref = p->GetOccupiedFurniture().get();
            if (alchemyMenu() || alchemyFurniture(ref.get())) {
                labVisit.confirmAlchemyMenu();
                SKSE::log::info("Lab Skeever: alchemy crafting menu opened");
            } else if (ref) labVisit.cancel();
        } else finishLab("alchemy menu closed");
        return RE::BSEventNotifyControl::kContinue;
    }
};
LabEvents labEvents;
RE::BGSAttackData* attackData(RE::Actor* actor) {
    auto process = actor ? actor->GetActorRuntimeData().currentProcess : nullptr;
    return process && process->high ? process->high->attackData.get() : nullptr;
}
bool twoHanded(RE::TESObjectWEAP* w) { return w && (w->IsTwoHandedSword() || w->IsTwoHandedAxe()); }
RE::TESObjectWEAP* weapon(RE::Actor* actor, RE::BGSAttackData* ad) {
    auto obj = actor->GetEquippedObject(ad && ad->IsLeftAttack());
    return obj ? obj->As<RE::TESObjectWEAP>() : nullptr;
}
void beginAttack(RE::Actor* actor, RE::BGSAttackData* ad) {
    if (!ad) return;
    auto w = weapon(actor, ad);
    const bool bash = ad->data.flags.any(RE::AttackData::AttackFlag::kBashAttack);
    const bool power = ad->data.flags.any(RE::AttackData::AttackFlag::kPowerAttack);
    combat.beginSwing(bash, power, !w || w->IsMelee(), twoHanded(w), selected(guard), selected(echo));
}
void observeAttack(RE::PlayerCharacter* p) {
    auto state = p->AsActorState()->GetAttackState();
    auto ad = attackData(p);
    const bool active = state == RE::ATTACK_STATE_ENUM::kSwing || state == RE::ATTACK_STATE_ENUM::kHit || state == RE::ATTACK_STATE_ENUM::kBash;
    const bool newPhase = active && (!combat.swing || ad != lastData ||
        (lastState == RE::ATTACK_STATE_ENUM::kFollowThrough || lastState == RE::ATTACK_STATE_ENUM::kNextAttack));
    if (newPhase) beginAttack(p, ad);
    if (state == RE::ATTACK_STATE_ENUM::kNone || state == RE::ATTACK_STATE_ENUM::kDraw) combat.endSwing();
    lastState = state; lastData = ad;
}
void update(RE::PlayerCharacter* p, float dt) {
    originalUpdate(p, dt);
    if (!ready || !session || !p) return;
    auto ui = RE::UI::GetSingleton();
    if (ui && ui->GameIsPaused()) return;
    std::lock_guard lock(stateMutex);
    const bool labSelected = selectedLab();
    if (!labSelected) {
        labVisit.cancel();
        if (hadLabTrait) { ++labEpoch; dispelLab(p); }
    }
    hadLabTrait = labSelected;
    combat.tick(dt);
    if (!selected(guard)) combat.clearGuard();
    if (!selected(echo)) combat.clearEcho();
    if (p->IsDead()) { combat = {}; labVisit.cancel(); return; }
    observeAttack(p);
}
void processHit(RE::Actor* target, RE::HitData& hit) {
    // This function also queues itself from worker jobs. Touch state only when
    // the engine is processing the actual game-thread hit, never its queued copy.
    if (!ready || !session || RE::TaskQueueInterface::ShouldUseTaskQueue()) { originalHit(target, hit); return; }
    auto p = RE::PlayerCharacter::GetSingleton();
    const auto attacker = hit.aggressor.get();
    const bool blocked = hit.flags.any(RE::HitData::Flag::kBlocked, RE::HitData::Flag::kBlockWithWeapon);
    const bool bash = hit.flags.any(RE::HitData::Flag::kBash, RE::HitData::Flag::kTimedBash);
    const bool power = hit.flags.any(RE::HitData::Flag::kPowerAttack);
    const bool melee = bash || (hit.weapon ? hit.weapon->IsMelee() : hit.flags.any(RE::HitData::Flag::kMeleeAttack));
    float mult = 1;
    {
        std::lock_guard lock(stateMutex);
        if (attacker.get() == p && target && target != p && melee) {
            // Collision mods may deliver a hit before the animation callback.
            // Begin its swing from authoritative HitData in that case.
            if (!combat.swing || combat.bash != bash || combat.power != power) {
                combat.beginSwing(bash, power, melee, twoHanded(hit.weapon), selected(guard), selected(echo));
                lastData = hit.attackData.get();
            }
            if ((bash && selected(guard)) || (!bash && selected(echo))) mult *= combat.damage(bash, power, melee);
        }
        if (target == p && attacker && attacker.get() != p && selected(guard)) {
            if (blocked) combat.block();
            else mult *= 1.15f;
        }
    }
    // Weapon/bashing HitData damage only. Spells, poison ticks, falls and the
    // weapon's separately applied enchantment effects never enter this path.
    if (mult != 1 && std::isfinite(hit.totalDamage) && hit.totalDamage > 0) {
        hit.totalDamage *= mult;
        hit.physicalDamage *= mult;
        hit.resistedPhysicalDamage *= mult;
    }
    originalHit(target, hit);
}
class Actions final : public RE::BSTEventSink<SKSE::ActionEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const SKSE::ActionEvent* event, RE::BSTEventSource<SKSE::ActionEvent>*) override {
        if (!event || !ready || !session || event->actor != RE::PlayerCharacter::GetSingleton()) return RE::BSEventNotifyControl::kContinue;
        std::lock_guard lock(stateMutex);
        if (event->type == SKSE::ActionEvent::Type::kVoiceFire && event->sourceForm && event->sourceForm->As<RE::TESShout>() && selected(echo)) combat.shout();
        if (event->type == SKSE::ActionEvent::Type::kWeaponSwing) {
            auto ad = attackData(event->actor);
            // The engine action callback precedes damage; a missed swing also
            // spends the token. Update detects bash starts and combo transitions.
            if (!combat.swing || ad != lastData) beginAttack(event->actor, ad);
            lastData = ad;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
Actions actions;

bool isBlessing(RE::ActiveEffect* ae) {
    return ae && ae->spell && ae->effect && ae->effect->baseEffect && blessing &&
        (ae->spell->HasKeyword(blessing) || ae->effect->baseEffect->HasKeyword(blessing));
}
thread_local std::unordered_set<RE::ActiveEffect*> adjusting;
template<class T> struct BlessingHook {
    inline static REL::Relocation<void (*)(RE::ActiveEffect*, RE::Actor*, RE::MagicTarget*)> original;
    static void adjust(RE::ActiveEffect* ae, RE::Actor* caster, RE::MagicTarget* target) {
        const bool outer = adjusting.insert(ae).second;
        original(ae, caster, target);
        if (!outer) return;
        adjusting.erase(ae);
        auto p = RE::PlayerCharacter::GetSingleton();
        if (!selected(burden) || !target || target->GetTargetAsActor() != p || !isBlessing(ae)) return;
        // GetTotalCarryWeight is the maximum capacity, NOT the inventory load.
        // Use the same load/capacity pair as OAR's inventory-weight condition.
        auto inventory = p->GetInventoryChanges(true);
        if (!inventory || !traits::lightLoad(inventory->GetInventoryWeight(), p->GetTotalCarryWeight())) return;
        const auto flags = ae->effect->baseEffect->data.flags;
        if (!flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude) && std::isfinite(ae->magnitude)) ae->magnitude *= 1.25f;
        if (ae->duration > 0 && std::isfinite(ae->duration)) ae->duration *= 2;
    }
    static void install() { REL::Relocation<std::uintptr_t> table{T::VTABLE[0]}; original = table.write_vfunc(0, adjust); }
};
void installBlessingHooks() {
    BlessingHook<RE::AbsorbEffect>::install();
    BlessingHook<RE::AccumulatingValueModifierEffect>::install();
    BlessingHook<RE::ActiveEffect>::install();
    BlessingHook<RE::BanishEffect>::install();
    BlessingHook<RE::BoundItemEffect>::install();
    BlessingHook<RE::CalmEffect>::install();
    BlessingHook<RE::CloakEffect>::install();
    BlessingHook<RE::CommandEffect>::install();
    BlessingHook<RE::CommandSummonedEffect>::install();
    BlessingHook<RE::ConcussionEffect>::install();
    BlessingHook<RE::CureEffect>::install();
    BlessingHook<RE::DarknessEffect>::install();
    BlessingHook<RE::DemoralizeEffect>::install();
    BlessingHook<RE::DetectLifeEffect>::install();
    BlessingHook<RE::DisarmEffect>::install();
    BlessingHook<RE::DisguiseEffect>::install();
    BlessingHook<RE::DispelEffect>::install();
    BlessingHook<RE::DualValueModifierEffect>::install();
    BlessingHook<RE::EnhanceWeaponEffect>::install();
    BlessingHook<RE::EtherealizationEffect>::install();
    BlessingHook<RE::FrenzyEffect>::install();
    BlessingHook<RE::GrabActorEffect>::install();
    BlessingHook<RE::GuideEffect>::install();
    BlessingHook<RE::InvisibilityEffect>::install();
    BlessingHook<RE::LightEffect>::install();
    BlessingHook<RE::LockEffect>::install();
    BlessingHook<RE::NightEyeEffect>::install();
    BlessingHook<RE::OpenEffect>::install();
    BlessingHook<RE::ParalysisEffect>::install();
    BlessingHook<RE::PeakValueModifierEffect>::install();
    BlessingHook<RE::RallyEffect>::install();
    BlessingHook<RE::ReanimateEffect>::install();
    BlessingHook<RE::ScriptEffect>::install();
    BlessingHook<RE::ScriptedRefEffect>::install();
    BlessingHook<RE::SlowTimeEffect>::install();
    BlessingHook<RE::SoulTrapEffect>::install();
    BlessingHook<RE::SpawnHazardEffect>::install();
    BlessingHook<RE::StaggerEffect>::install();
    BlessingHook<RE::SummonCreatureEffect>::install();
    BlessingHook<RE::TelekinesisEffect>::install();
    BlessingHook<RE::TurnUndeadEffect>::install();
    BlessingHook<RE::ValueAndConditionsEffect>::install();
    BlessingHook<RE::ValueModifierEffect>::install();
    BlessingHook<RE::VampireLordEffect>::install();
    BlessingHook<RE::WerewolfEffect>::install();
    BlessingHook<RE::WerewolfFeedEffect>::install();
}
void message(SKSE::MessagingInterface::Message* msg) {
    if (msg->type == SKSE::MessagingInterface::kDataLoaded) {
        auto data = RE::TESDataHandler::GetSingleton();
        burden = data->LookupForm<RE::SpellItem>(0xF00, pluginFile);
        guard = data->LookupForm<RE::SpellItem>(0xF10, pluginFile);
        echo = data->LookupForm<RE::SpellItem>(0xF20, pluginFile);
        blessing = RE::TESForm::LookupByID<RE::BGSKeyword>(0xFB98C);
        labTrait = data->LookupForm<RE::SpellItem>(0xE00, pluginFile);
        labBonus = data->LookupForm<RE::SpellItem>(0xE03, pluginFile);
        labBonusEffect = data->LookupForm<RE::EffectSetting>(0xE04, pluginFile);
        if (labTrait && labBonus && labBonusEffect) {
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESFurnitureEvent>(&labEvents);
            RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&labEvents);
            SKSE::log::info("Lab Skeever ready: native alchemy workbench/menu detection; 20 seconds, 30x duration, 1.1x potency");
        } else SKSE::log::error("Lab Skeever: required forms missing; activation disabled");
        if (!burden || !guard || !echo || !blessing) { SKSE::log::error("Combined v2.4 forms missing; helper disabled"); return; }
        // Actor::ProcessHitData (all ordinary physical hits, including arrows).
        // ID and signature corroborated by Acheron and Valhalla Combat sources.
        auto address = REL::Relocation<std::uintptr_t>{REL::RelocationID(37633,38586)}.address();
        const auto init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) { SKSE::log::error("MinHook init failed {}", int(init)); return; }
        if (auto s = MH_CreateHook(reinterpret_cast<void*>(address), reinterpret_cast<void*>(processHit), reinterpret_cast<void**>(&originalHit)); s != MH_OK) { SKSE::log::error("Hit hook creation failed {}", int(s)); return; }
        if (auto s = MH_EnableHook(reinterpret_cast<void*>(address)); s != MH_OK) { SKSE::log::error("Hit hook enable failed {}", int(s)); return; }
        REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
        originalUpdate = table.write_vfunc(0xAD, update);
        installBlessingHooks();
        SKSE::GetActionEventSource()->AddEventSink(&actions);
        ready = true;
        SKSE::log::info("Ready: Burden of Devotion, Unbroken Guard, Echoing Steel");
    } else if (msg->type == SKSE::MessagingInterface::kPreLoadGame) { session = false; reset(); }
    else if (msg->type == SKSE::MessagingInterface::kNewGame) { reset(); session = true; }
    else if (msg->type == SKSE::MessagingInterface::kPostLoadGame) { reset(); session = msg->data != nullptr; }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData d{}; d.PluginVersion({1,1,0,0}); d.PluginName("BiggieTraitMechanics");
    d.AuthorName("Physics-helper contributors"); d.UsesAddressLibrary(true); d.UsesStructsPost629(true);
    d.CompatibleVersions({REL::Version{1,6,1170,0}}); return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if (skse->RuntimeVersion() != REL::Version{1,6,1170,0}) return false;
    auto path = SKSE::log::log_directory(); if (!path) return false; *path /= "BiggieTraitMechanics.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global", std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true)));
    spdlog::set_level(spdlog::level::info); spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("BiggieTraitMechanics 1.1.0; Skyrim 1.6.1170");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
