#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "Harvest.h"
#include <atomic>
#include <mutex>

namespace
{
    constexpr auto traitFile = "Biggie Traits - Devoted Alchemist.esp";
    constexpr auto phialFile = "The White Phial - Tweaks and Enhancements.esp";
    constexpr std::uint32_t saveID = 0x56484D31;  // VHM1, dedicated SKSE co-save section
    constexpr std::uint32_t recordID = 0x48415256;
    RE::SpellItem* trait = nullptr;
    RE::BGSListForm* retained = nullptr;
    RE::AlchemyItem* phialPoison = nullptr;
    RE::BGSListForm* phials = nullptr;
    RE::BGSListForm* selectedLiquid = nullptr;
    RE::EffectSetting* refillPoison = nullptr;
    RE::EffectSetting* refillPotion = nullptr;
    std::atomic_bool ready = false, session = false, queued = false;
    std::atomic<std::uint64_t> epoch = 0;
    std::mutex mutex;
    harvest::Ledger ledger;
    std::set<harvest::ID> pins;

    bool selected()
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        return ready.load() && session.load() && player && trait && player->HasSpell(trait);
    }

    bool isPhial(RE::AlchemyItem* item)
    {
        return item && (item == phialPoison || (phials && phials->HasForm(item)));
    }

    bool marker(RE::Effect* effect)
    {
        return effect && effect->baseEffect &&
            (effect->baseEffect == refillPoison || effect->baseEffect == refillPotion);
    }

    bool sameLiquid(RE::AlchemyItem* a, RE::AlchemyItem* b)
    {
        // The original phial copies its selected liquid's EFIT/EFID list, then
        // adds a refill marker. Never guess the returned poison from its name.
        std::vector<RE::Effect*> left, right;
        for (auto e : a->effects) if (e && !marker(e)) left.push_back(e);
        for (auto e : b->effects) if (e && !marker(e)) right.push_back(e);
        if (left.empty() || left.size() != right.size()) return false;
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (left[i]->baseEffect != right[i]->baseEffect ||
                left[i]->effectItem.magnitude != right[i]->effectItem.magnitude ||
                left[i]->effectItem.duration != right[i]->effectItem.duration ||
                left[i]->effectItem.area != right[i]->effectItem.area) return false;
        }
        return true;
    }

    RE::AlchemyItem* bottleFor(RE::AlchemyItem* source)
    {
        if (!source || !source->IsPoison()) return nullptr;
        if (!isPhial(source)) return source;
        if (source != phialPoison || !selectedLiquid) return nullptr;
        RE::AlchemyItem* result = nullptr;
        selectedLiquid->ForEachForm([&](RE::TESForm* form) {
            auto item = form ? form->As<RE::AlchemyItem>() : nullptr;
            if (item && item->IsPoison() && !isPhial(item) && sameLiquid(source, item)) result = item;
            return RE::BSContainer::ForEachResult::kStop;
        });
        return result;
    }

    bool ownedPoison(RE::ActiveEffect* effect, RE::Actor* caster = nullptr, RE::Actor* target = nullptr)
    {
        if (!effect || !effect->spell || !effect->effect || !effect->effect->baseEffect) return false;
        auto item = effect->spell->As<RE::AlchemyItem>();
        if (!item || !item->IsPoison()) return false;
        const auto base = effect->effect->baseEffect;
        if ((!base->IsHostile() && !base->IsDetrimental()) || marker(effect->effect)) return false;
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto sourceActor = caster ? RE::NiPointer<RE::Actor>(caster) : effect->GetCasterActor();
        if (!target) target = effect->GetTargetActor();
        return player && sourceActor.get() == player && target && target != player && selected();
    }

    harvest::Key keyFor(RE::ActiveEffect* effect)
    {
        const auto actor = effect ? effect->GetTargetActor() : nullptr;
        const auto base = effect ? effect->GetBaseObject() : nullptr;
        if (!actor || !base || !effect->spell) return {};
        return { actor->GetFormID(), effect->spell->GetFormID(), base->GetFormID(), effect->usUniqueID };
    }

    bool live(RE::ActiveEffect* effect, bool executing = false, bool starting = false)
    {
        if (!effect || !effect->effect || !effect->effect->baseEffect) return false;
        const bool noMagnitude = effect->effect->baseEffect->data.flags.any(
            RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude);
        if (noMagnitude ? effect->duration <= 0 : !std::isfinite(effect->magnitude) || effect->magnitude == 0) return false;
        return harvest::live(effect->flags.any(RE::ActiveEffect::Flag::kDispelled),
            !starting && effect->flags.any(RE::ActiveEffect::Flag::kInactive),
            effect->conditionStatus == RE::ActiveEffect::ConditionStatus::kFalse,
            effect->elapsedSeconds, effect->duration, executing);
    }

    void queueWork();

    std::optional<harvest::Application> track(RE::ActiveEffect* effect, bool start)
    {
        if (!ownedPoison(effect)) return std::nullopt;
        const auto key = keyFor(effect);
        if (!key.actor) return std::nullopt;
        if (!start) {
            std::lock_guard lock(mutex);
            if (const auto app = ledger.find(key)) return *app;
            // Some load paths allocate a new active-effect instance ID. Rebind
            // an existing saved application; do not invent a new phial mapping.
            for (auto it = ledger.effects.begin(); it != ledger.effects.end(); ++it) {
                if (it->first.actor == key.actor && it->first.source == key.source && it->first.effect == key.effect) {
                    auto app = it->second;
                    app.key = key;
                    ledger.effects.erase(it);
                    ledger.effects[key] = app;
                    return app;
                }
            }
            return std::nullopt;
        }
        auto bottle = bottleFor(effect->spell->As<RE::AlchemyItem>());
        if (!bottle) return std::nullopt;
        std::optional<harvest::Application> result;
        {
            std::lock_guard lock(mutex);
            ledger.begin(key, bottle->GetFormID(), true);
            if (const auto app = ledger.find(key)) result = *app;
            if ((bottle->GetFormID() >> 24) == 0xFF) pins.insert(bottle->GetFormID());
        }
        queueWork();
        return result;
    }

    struct Execution;
    thread_local Execution* executing = nullptr;

    struct Execution
    {
        Execution* previous = executing;
        std::optional<harvest::Application> app;
        RE::NiPointer<RE::Actor> actor;
        bool eligible = false, aliveBefore = false;
        std::uint64_t generation = epoch.load();

        Execution(RE::ActiveEffect* effect, bool start) : app(track(effect, start))
        {
            if (app) {
                actor.reset(effect->GetTargetActor());
                eligible = live(effect, true, start);
                aliveBefore = actor && actor->GetLifeState() == RE::ACTOR_LIFE_STATE::kAlive;
            }
            executing = this;
        }
        ~Execution()
        {
            executing = previous;
            // The engine may destroy the ActiveEffect inside Start/Update.
            // Only retained actor references and copied IDs are used here.
            if (!app || !actor || !eligible || !aliveBefore || generation != epoch.load() || !selected()) return;
            const auto state = actor->GetLifeState();
            if (state != RE::ACTOR_LIFE_STATE::kDying && state != RE::ACTOR_LIFE_STATE::kDead) return;
            {
                std::lock_guard lock(mutex);
                ledger.offer(*app, true);  // Death occurred inside this player's poison callback.
            }
            queueWork();
        }
    };

    void endEffect(RE::ActiveEffect* effect)
    {
        if (!ready.load() || !session.load()) return;
        const auto key = keyFor(effect);
        if (!key.actor) return;
        const auto actor = effect->GetTargetActor();
        const auto state = actor->GetLifeState();
        const bool dying = state == RE::ACTOR_LIFE_STATE::kDying || state == RE::ACTOR_LIFE_STATE::kDead;
        std::lock_guard lock(mutex);
        ledger.end(key, dying && live(effect));
    }

    template<class T>
    struct Hook
    {
        inline static REL::Relocation<void (*)(RE::ActiveEffect*, RE::Actor*, RE::MagicTarget*)> adjust;
        inline static REL::Relocation<void (*)(RE::ActiveEffect*)> start, finish, remove;
        inline static REL::Relocation<void (*)(RE::ActiveEffect*, float)> update;

        static void Adjust(RE::ActiveEffect* effect, RE::Actor* caster, RE::MagicTarget* target)
        {
            adjust(effect, caster, target);
            if (!ownedPoison(effect, caster, target ? target->GetTargetAsActor() : nullptr)) return;
            harvest::weaken(effect->magnitude, effect->duration,
                effect->effect->baseEffect->data.flags.any(RE::EffectSetting::EffectSettingData::Flag::kNoMagnitude));
        }
        static void Start(RE::ActiveEffect* effect)
        {
            Execution scope(effect, true);
            start(effect);
        }
        static void Update(RE::ActiveEffect* effect, float delta)
        {
            Execution scope(effect, false);
            update(effect, delta);
        }
        static void Finish(RE::ActiveEffect* effect) { endEffect(effect); finish(effect); }
        static void Remove(RE::ActiveEffect* effect) { endEffect(effect); remove(effect); }
        static void install()
        {
            REL::Relocation<std::uintptr_t> table{ T::VTABLE[0] };
            adjust = table.write_vfunc(0x00, Adjust);
            remove = table.write_vfunc(0x02, Remove);
            update = table.write_vfunc(0x04, Update);
            start = table.write_vfunc(0x14, Start);
            finish = table.write_vfunc(0x15, Finish);
        }
    };

    void installHooks()
    {
        // Unique originals per concrete vtable; preserve preceding hooks.
        // These are documented virtual slots, not guessed instruction offsets.
        Hook<RE::AbsorbEffect>::install();
        Hook<RE::AccumulatingValueModifierEffect>::install();
        Hook<RE::ActiveEffect>::install();
        Hook<RE::BanishEffect>::install();
        Hook<RE::BoundItemEffect>::install();
        Hook<RE::CalmEffect>::install();
        Hook<RE::CloakEffect>::install();
        Hook<RE::CommandEffect>::install();
        Hook<RE::CommandSummonedEffect>::install();
        Hook<RE::ConcussionEffect>::install();
        Hook<RE::CureEffect>::install();
        Hook<RE::DarknessEffect>::install();
        Hook<RE::DemoralizeEffect>::install();
        Hook<RE::DetectLifeEffect>::install();
        Hook<RE::DisarmEffect>::install();
        Hook<RE::DisguiseEffect>::install();
        Hook<RE::DispelEffect>::install();
        Hook<RE::DualValueModifierEffect>::install();
        Hook<RE::EnhanceWeaponEffect>::install();
        Hook<RE::EtherealizationEffect>::install();
        Hook<RE::FrenzyEffect>::install();
        Hook<RE::GrabActorEffect>::install();
        Hook<RE::GuideEffect>::install();
        Hook<RE::InvisibilityEffect>::install();
        Hook<RE::LightEffect>::install();
        Hook<RE::LockEffect>::install();
        Hook<RE::NightEyeEffect>::install();
        Hook<RE::OpenEffect>::install();
        Hook<RE::ParalysisEffect>::install();
        Hook<RE::PeakValueModifierEffect>::install();
        Hook<RE::RallyEffect>::install();
        Hook<RE::ReanimateEffect>::install();
        Hook<RE::ScriptEffect>::install();
        Hook<RE::ScriptedRefEffect>::install();
        Hook<RE::SlowTimeEffect>::install();
        Hook<RE::SoulTrapEffect>::install();
        Hook<RE::SpawnHazardEffect>::install();
        Hook<RE::StaggerEffect>::install();
        Hook<RE::SummonCreatureEffect>::install();
        Hook<RE::TelekinesisEffect>::install();
        Hook<RE::TurnUndeadEffect>::install();
        Hook<RE::ValueAndConditionsEffect>::install();
        Hook<RE::ValueModifierEffect>::install();
        Hook<RE::VampireLordEffect>::install();
        Hook<RE::WerewolfEffect>::install();
        Hook<RE::WerewolfFeedEffect>::install();
    }

    void flush(std::uint64_t generation)
    {
        if (generation != epoch.load()) return;
        queued.store(false);
        if (!ready.load() || !session.load()) return;
        std::set<harvest::ID> toPin;
        std::vector<harvest::ID> victims;
        {
            std::lock_guard lock(mutex);
            toPin.swap(pins);
            for (const auto& [actor, candidate] : ledger.candidates) victims.push_back(actor);
        }
        for (auto id : toPin) {
            auto item = RE::TESForm::LookupByID<RE::AlchemyItem>(id);
            if (item && !isPhial(item) && item->IsPoison() && !retained->HasForm(item)) retained->AddForm(item);
        }
        const bool enabled = selected();
        for (auto id : victims) {
            auto actor = RE::TESForm::LookupByID<RE::Actor>(id);
            if (!actor) continue;  // Unloaded references can resolve on a later poll.
            const auto state = actor->GetLifeState();
            if (state == RE::ACTOR_LIFE_STATE::kDying) continue;
            std::optional<harvest::ID> bottle;
            {
                std::lock_guard lock(mutex);
                if (generation != epoch.load()) return;
                if (!enabled || state != RE::ACTOR_LIFE_STATE::kDead) {
                    ledger.candidates.erase(id);  // Canceled death/essential actor: no reward.
                    continue;
                }
                bottle = ledger.claim(id);
            }
            if (!bottle) continue;
            auto item = RE::TESForm::LookupByID<RE::AlchemyItem>(*bottle);
            auto player = RE::PlayerCharacter::GetSingleton();
            if (player && item && item->IsPoison() && !isPhial(item)) {
                player->AddObjectToContainer(item, nullptr, 1, nullptr);
                SKSE::log::info("Recovered poison {:08X} from victim {:08X}", *bottle, id);
            } else SKSE::log::warn("Skipped unresolved/non-poison reward {:08X} for {:08X}", *bottle, id);
        }
    }

    void queueWork()
    {
        if (!ready.load() || !session.load() || queued.exchange(true)) return;
        const auto generation = epoch.load();
        SKSE::GetTaskInterface()->AddTask([generation] { flush(generation); });
    }

    void poll(RE::StaticFunctionTag*) { queueWork(); }

    class Events final : public RE::BSTEventSink<RE::TESDeathEvent>, public RE::BSTEventSink<RE::TESFormDeleteEvent>
    {
    public:
        using Result = RE::BSEventNotifyControl;
        static Events& get() { static Events events; return events; }
        Result ProcessEvent(const RE::TESDeathEvent* event, RE::BSTEventSource<RE::TESDeathEvent>*) override
        {
            if (!event || !event->actorDying || !selected()) return Result::kContinue;
            auto actor = event->actorDying->As<RE::Actor>();
            const auto player = RE::PlayerCharacter::GetSingleton();
            if (!actor || actor == player) return Result::kContinue;
            const auto id = actor->GetFormID();
            bool playerKiller = event->actorKiller.get() == player;
            std::vector<harvest::Key> eligible;
            if (auto effects = actor->GetActiveEffectList()) {
                for (auto effect : *effects) if (ownedPoison(effect) && live(effect)) eligible.push_back(keyFor(effect));
            }
            std::vector<harvest::Application> immediate;
            for (auto scope = executing; scope; scope = scope->previous) {
                if (!scope->app || !scope->eligible || scope->app->key.actor != id) continue;
                immediate.push_back(*scope->app);
                // Null killer may be reported for an instant poison. Require a
                // death inside that exact player-owned callback, never mere proximity.
                if (!event->actorKiller && scope->aliveBefore) playerKiller = true;
            }
            {
                std::lock_guard lock(mutex);
                for (const auto& app : immediate) if (playerKiller) ledger.offer(app, true);
                // OnDying and OnDeath can carry different killer fields. A
                // confirmed earlier event must survive a later null-killer event.
                const auto pending = ledger.candidates.find(id);
                if (playerKiller || pending == ledger.candidates.end() || !pending->second.confirmed)
                    ledger.death(id, playerKiller, eligible);
            }
            queueWork();
            return Result::kContinue;
        }
        Result ProcessEvent(const RE::TESFormDeleteEvent* event, RE::BSTEventSource<RE::TESFormDeleteEvent>*) override
        {
            if (event) { std::lock_guard lock(mutex); ledger.forget(event->formID); }
            return Result::kContinue;
        }
    };

    void reset()
    {
        ++epoch; queued.store(false);
        std::lock_guard lock(mutex);
        ledger.clear(); pins.clear();
    }

    void save(SKSE::SerializationInterface* api)
    {
        std::lock_guard lock(mutex);
        const auto bytes = harvest::encode(ledger);
        if (!api->WriteRecord(recordID, 1, bytes.data(), static_cast<std::uint32_t>(bytes.size())))
            SKSE::log::error("Could not write Venom Harvester save state");
    }

    void load(SKSE::SerializationInterface* api)
    {
        reset();
        std::uint32_t type = 0, version = 0, length = 0;
        while (api->GetNextRecordInfo(type, version, length)) {
            if (type != recordID || version != 1 || length > 64 * 1024 * 1024) continue;
            std::vector<std::uint8_t> bytes(length);
            if (api->ReadRecordData(bytes.data(), length) != length) continue;
            auto restored = harvest::decode(bytes, [api](harvest::ID oldID) {
                RE::FormID id = 0;
                return api->ResolveFormID(oldID, id) ? id : 0;
            });
            if (restored) {
                std::lock_guard lock(mutex);
                ledger = std::move(*restored);
                SKSE::log::info("Loaded {} poison effects, {} pending and {} completed harvests",
                    ledger.effects.size(), ledger.candidates.size(), ledger.rewarded.size());
            } else SKSE::log::error("Rejected invalid Venom Harvester save state");
        }
    }

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (message->type == SKSE::MessagingInterface::kDataLoaded) {
            auto data = RE::TESDataHandler::GetSingleton();
            trait = data->LookupForm<RE::SpellItem>(0x800, traitFile);
            retained = data->LookupForm<RE::BGSListForm>(0x806, traitFile);
            const auto migration = data->LookupForm<RE::TESGlobal>(0x805, traitFile);
            phialPoison = data->LookupForm<RE::AlchemyItem>(0x80C, phialFile);
            phials = data->LookupForm<RE::BGSListForm>(0x817, phialFile);
            selectedLiquid = data->LookupForm<RE::BGSListForm>(0xD4A, phialFile);
            refillPoison = data->LookupForm<RE::EffectSetting>(0x80B, phialFile);
            refillPotion = data->LookupForm<RE::EffectSetting>(0x9D6, "Update.esm");
            ready.store(trait && retained && migration);
            if (!ready.load()) {
                SKSE::log::error("Disabled: enable the Venom Harvester ESP from Biggie Traits Combined v1.6 or later");
                return;
            }
            auto source = RE::ScriptEventSourceHolder::GetSingleton();
            source->AddEventSink<RE::TESDeathEvent>(&Events::get());
            source->AddEventSink<RE::TESFormDeleteEvent>(&Events::get());
            installHooks();
            SKSE::log::info("Ready; trait {:08X}; White Phial mapping available={}", trait->GetFormID(),
                phialPoison && phials && selectedLiquid && refillPoison && refillPotion);
        } else if (message->type == SKSE::MessagingInterface::kPreLoadGame) {
            session.store(false); reset();
        } else if (message->type == SKSE::MessagingInterface::kNewGame) {
            reset(); session.store(true); queueWork();
        } else if (message->type == SKSE::MessagingInterface::kPostLoadGame) {
            session.store(message->data != nullptr); queueWork();
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({ 1, 0, 0, 0 });
    data.PluginName("VenomHarvester");
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
    *path /= "VenomHarvester.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("VenomHarvester 1.0.0; Skyrim 1.6.1170; instance penalty 0.75; one poison per victim");
    auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(saveID);
    serialization->SetSaveCallback(save);
    serialization->SetLoadCallback(load);
    serialization->SetRevertCallback([](SKSE::SerializationInterface*) { reset(); });
    if (!SKSE::GetPapyrusInterface()->Register([](RE::BSScript::IVirtualMachine* vm) {
        vm->RegisterFunction("Poll", "VH_Native", poll);
        return true;
    })) return false;
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
