#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_set>

#include "RewardState.h"

namespace
{
    constexpr auto mod = "BOOB.esp";
    constexpr std::uint32_t signature = 0x534F5641; // SOVA
    std::atomic_bool ready{false};
    std::atomic_bool session{false};
    std::atomic_uint64_t generation{0};
    std::mutex stateMutex;
    ovation::State state;
    // Only read or modified on the game thread after loading is complete.
    std::unordered_set<RE::FormID> towns;

    template <class T>
    T* form(std::uint32_t id)
    {
        const auto data = RE::TESDataHandler::GetSingleton();
        return data ? data->LookupForm<T>(id, mod) : nullptr;
    }

    bool validForms()
    {
        if (!form<RE::BGSPerk>(0x818)) return false;
        for (std::size_t i = 0; i < ovation::actorValues.size(); ++i) {
            const auto spell = form<RE::SpellItem>(ovation::spellStart + static_cast<std::uint32_t>(i));
            const auto effect = form<RE::EffectSetting>(ovation::effectStart + static_cast<std::uint32_t>(i));
            if (!spell || !effect || spell->effects.size() != 1 || !spell->effects[0] ||
                spell->effects[0]->baseEffect != effect ||
                static_cast<std::uint32_t>(effect->data.primaryAV) != ovation::actorValues[i] ||
                spell->GetSpellType() != RE::MagicSystem::SpellType::kAbility ||
                spell->GetCastingType() != RE::MagicSystem::CastingType::kConstantEffect)
                return false;
        }
        return true;
    }

    bool keyword(const RE::BGSLocation* loc, std::string_view name)
    {
        return loc && loc->HasKeywordString(name);
    }

    bool settlement(const RE::BGSLocation* loc)
    {
        return loc && !keyword(loc, "LocTypeInn") &&
            (keyword(loc, "LocTypeCity") || keyword(loc, "LocTypeTown") ||
                keyword(loc, "LocTypeSettlement") || keyword(loc, "LocTypeHabitationHasInn"));
    }

    void findTowns()
    {
        towns.clear();
        const auto data = RE::TESDataHandler::GetSingleton();
        if (!data) return;
        for (const auto location : data->GetFormArray<RE::BGSLocation>()) {
            if (!location) continue;
            if (settlement(location) && keyword(location, "LocTypeHabitationHasInn"))
                towns.insert(location->GetFormID());
            if (!keyword(location, "LocTypeInn")) continue;
            // Discover mod-added inns too. Never use an entire hold as a town.
            auto parent = location->parentLoc;
            for (unsigned depth = 0; parent && depth < 64; ++depth, parent = parent->parentLoc) {
                if (keyword(parent, "LocTypeHold")) break;
                if (settlement(parent)) {
                    towns.insert(parent->GetFormID());
                    break;
                }
            }
        }
        SKSE::log::info("Found {} town/city locations with inns.", towns.size());
    }

    RE::FormID townOf(RE::BGSLocation* location)
    {
        for (unsigned depth = 0; location && depth < 64; ++depth, location = location->parentLoc) {
            if (towns.contains(location->GetFormID())) return location->GetFormID();
            if (keyword(location, "LocTypeHold")) break;
        }
        return 0;
    }

    void removeReward(RE::PlayerCharacter* player)
    {
        if (!player) return;
        for (std::uint32_t i = 0; i < ovation::actorValues.size(); ++i)
            if (const auto spell = form<RE::SpellItem>(ovation::spellStart + i))
                player->RemoveSpell(spell);
    }

    void configureAmounts(const ovation::Amounts& amounts)
    {
        // These are private, constant abilities with fixed actor values. No
        // shared potion, source MGEF, actor base value, or unrelated spell changes.
        for (std::uint32_t i = 0; i < amounts.size(); ++i) {
            const auto spell = form<RE::SpellItem>(ovation::spellStart + i);
            if (spell && spell->effects.size() == 1 && spell->effects[0])
                spell->effects[0]->effectItem.magnitude = amounts[i];
        }
    }

    bool restoreAbilities(RE::PlayerCharacter* player, const ovation::Amounts& amounts)
    {
        bool success = true;
        for (std::uint32_t i = 0; i < amounts.size(); ++i) {
            const auto spell = form<RE::SpellItem>(ovation::spellStart + i);
            if (!spell) return false;
            if (amounts[i] > 0.0f) {
                if (!player->HasSpell(spell)) success = player->AddSpell(spell) && success;
            } else {
                player->RemoveSpell(spell);
            }
        }
        return success;
    }

    bool rewardAmounts(RE::AlchemyItem* reward, ovation::Amounts& amounts)
    {
        amounts = {};
        if (!reward || reward->effects.empty()) return false;
        bool any = false;
        for (const auto effect : reward->effects) {
            if (!effect || !effect->baseEffect) return false;
            const auto base = effect->baseEffect;
            const auto type = base->GetArchetype();
            // The supplied script passes only Positive1/Positive2. Reject
            // unfamiliar/scripted rewards intact instead of partially converting.
            if (base->IsHostile() || base->IsDetrimental() ||
                (type != RE::EffectArchetype::kValueModifier && type != RE::EffectArchetype::kPeakValueModifier) ||
                !base->data.flags.all(RE::EffectSetting::EffectSettingData::Flag::kRecover) ||
                effect->conditions.head || base->conditions.head ||
                !ovation::addReward(amounts, static_cast<std::uint32_t>(base->data.primaryAV), effect->effectItem.magnitude))
                return false;
            any = any || effect->effectItem.magnitude > 0.0f;
        }
        return any;
    }

    void originalReward(RE::PlayerCharacter* player, RE::AlchemyItem* reward)
    {
        if (player && reward)
            RE::ActorEquipManager::GetSingleton()->EquipObject(player, reward, nullptr, 1,
                nullptr, false, false, false, true);
    }

    std::uint32_t rewardSlots(RE::AlchemyItem* reward)
    {
        std::uint32_t mask{};
        if (!reward) return mask;
        for (const auto effect : reward->effects) {
            if (!effect || !effect->baseEffect) continue;
            if (const auto index = ovation::slot(static_cast<std::uint32_t>(effect->baseEffect->data.primaryAV)))
                mask |= 1u << *index;
        }
        return mask;
    }

    void grant(RE::FormID rewardID, RE::FormID alternateID)
    {
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto reward = RE::TESForm::LookupByID<RE::AlchemyItem>(rewardID);
        const auto perk = form<RE::BGSPerk>(0x818);
        ovation::Amounts amounts{};
        if (!player || !perk || !player->HasPerk(perk) || !rewardAmounts(reward, amounts)) {
            originalReward(player, reward);
            return;
        }
        std::lock_guard lock(stateMutex);
        const auto alternate = RE::TESForm::LookupByID<RE::AlchemyItem>(alternateID);
        const auto replaced = rewardSlots(reward) | rewardSlots(alternate);
        const auto combined = ovation::mergeReward(state.active ? state.amounts : ovation::Amounts{}, amounts, replaced);
        // Replace this instrument's reward, preserving other instrument stats.
        for (std::uint32_t i = 0; i < amounts.size(); ++i)
            if (replaced & (1u << i))
                player->RemoveSpell(form<RE::SpellItem>(ovation::spellStart + i));
        // Clear the original two inn rewards, including a timed pre-update
        // reward. Kyne's Peace and unrelated potions are never dispelled.
        const auto target = player->GetMagicTarget();
        if (const auto effects = target ? target->GetActiveEffectList() : nullptr) {
            for (auto effect : *effects) {
                if (!effect || !effect->spell) continue;
                const auto id = effect->spell->GetFormID();
                if (id == rewardID || (alternateID && id == alternateID)) effect->Dispel(true);
            }
        }
        configureAmounts(combined);
        if (!restoreAbilities(player, combined)) {
            removeReward(player);
            state = {};
            SKSE::log::error("Failed to add constant reward; using the original reward.");
            originalReward(player, reward);
            return;
        }
        state.award(combined, townOf(player->GetCurrentLocation()));
        SKSE::log::info("Awarded 5x inn reward {:08X}; town={:08X}; health={} magicka={} stamina={} speech={} speechMod={} barter={}",
            rewardID, state.lastTown, amounts[0], amounts[1], amounts[2], amounts[3], amounts[4], amounts[5]);
    }

    bool awardInnReward(RE::StaticFunctionTag*, RE::AlchemyItem* reward, RE::AlchemyItem* alternate)
    {
        if (!ready.load() || !session.load() || !reward) return false;
        const auto player = RE::PlayerCharacter::GetSingleton();
        const auto perk = form<RE::BGSPerk>(0x818);
        if (!player || !perk || !player->HasPerk(perk)) return false;
        ovation::Amounts amounts{};
        if (!rewardAmounts(reward, amounts)) {
            SKSE::log::error("Unsupported inn reward {:08X}; original reward retained. Send this log and SkyrimsGotTalent-Bards.esp for diagnosis.", reward->GetFormID());
            return false;
        }
        const auto tasks = SKSE::GetTaskInterface();
        if (!tasks) return false;
        const auto epoch = generation.load();
        const auto rewardID = reward->GetFormID();
        const auto alternateID = alternate ? alternate->GetFormID() : 0;
        tasks->AddTask([epoch, rewardID, alternateID] {
            if (ready.load() && session.load() && epoch == generation.load())
                grant(rewardID, alternateID);
        });
        return true;
    }

    bool registerPapyrus(RE::BSScript::IVirtualMachine* vm)
    {
        vm->RegisterFunction("AwardInnReward", "BOOBStandingOvation", awardInnReward);
        return true;
    }

    class Events final : public RE::BSTEventSink<RE::TESActorLocationChangeEvent>
    {
    public:
        static Events& get() { static Events instance; return instance; }
        RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event,
            RE::BSTEventSource<RE::TESActorLocationChangeEvent>*) override
        {
            if (!event || !session.load() || !ready.load() ||
                event->actor.get() != RE::PlayerCharacter::GetSingleton())
                return RE::BSEventNotifyControl::kContinue;
            const auto locationID = event->newLoc ? event->newLoc->GetFormID() : 0;
            const auto epoch = generation.load();
            SKSE::GetTaskInterface()->AddTask([epoch, locationID] {
                if (!session.load() || epoch != generation.load()) return;
                const auto player = RE::PlayerCharacter::GetSingleton();
                const auto perk = form<RE::BGSPerk>(0x818);
                if (!player) return;
                const auto town = townOf(RE::TESForm::LookupByID<RE::BGSLocation>(locationID));
                std::lock_guard lock(stateMutex);
                if (!state.active) return;
                if (!perk || !player->HasPerk(perk) || state.enter(town)) {
                    removeReward(player);
                    state = {};
                    SKSE::log::info("Reward ended on location change; destination town={:08X}", town);
                }
            });
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void save(SKSE::SerializationInterface* io)
    {
        std::lock_guard lock(stateMutex);
        if (!io->WriteRecord(signature, 1, state)) SKSE::log::error("Cannot write Standing Ovation co-save.");
    }

    void revert(SKSE::SerializationInterface*)
    {
        session.store(false);
        generation.fetch_add(1);
        std::lock_guard lock(stateMutex);
        state = {};
    }

    void load(SKSE::SerializationInterface* io)
    {
        std::lock_guard lock(stateMutex);
        state = {};
        std::uint32_t type{}, version{}, length{};
        while (io->GetNextRecordInfo(type, version, length)) {
            if (type != signature || version != 1 || length != sizeof(ovation::State)) continue;
            ovation::State loaded{};
            if (io->ReadRecordData(&loaded, sizeof(loaded)) != sizeof(loaded) || !loaded.valid()) {
                SKSE::log::error("Invalid Standing Ovation save state; reward will be cleared.");
                continue;
            }
            if (loaded.lastTown) {
                RE::FormID resolved{};
                if (io->ResolveFormID(loaded.lastTown, resolved)) loaded.lastTown = resolved;
                else loaded.lastTown = 0;
            }
            state = loaded;
        }
        // SKSE form edits are not saved by the engine; restore the saved award
        // amounts every load, including loads of another character in one process.
        if (ready.load()) configureAmounts(state.amounts);
    }

    void startSession(bool newGame)
    {
        const auto epoch = generation.load();
        SKSE::GetTaskInterface()->AddTask([epoch, newGame] {
            if (!ready.load() || epoch != generation.load()) return;
            findTowns();
            const auto player = RE::PlayerCharacter::GetSingleton();
            const auto perk = form<RE::BGSPerk>(0x818);
            if (!player) return;
            std::lock_guard lock(stateMutex);
            if (newGame || !state.active || !perk || !player->HasPerk(perk)) {
                removeReward(player);
                state = {};
            } else {
                configureAmounts(state.amounts);
                if (!restoreAbilities(player, state.amounts)) {
                    removeReward(player);
                    state = {};
                    SKSE::log::error("Could not restore reward abilities.");
                }
            }
            // Loading a save is not arriving in a town. Establish the current
            // baseline before enabling gameplay location events.
            state.lastTown = townOf(player->GetCurrentLocation());
            session.store(true);
            SKSE::log::info("Session ready; active={} town={:08X}", state.active, state.lastTown);
        });
    }

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        switch (message->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            ready.store(validForms());
            if (!ready.load()) {
                SKSE::log::error("The matching updated BOOB.esp is missing or overridden. Install the complete Standing Ovation update.");
                break;
            }
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESActorLocationChangeEvent>(&Events::get());
            SKSE::log::info("Required perk and constant abilities verified.");
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            session.store(false);
            generation.fetch_add(1);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            if (message->data) startSession(false);
            break;
        case SKSE::MessagingInterface::kNewGame:
            session.store(false);
            generation.fetch_add(1);
            startSession(true);
            break;
        default:
            break;
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({1, 1, 0, 0});
    data.PluginName("StandingOvation");
    data.AuthorName("Physics-helper contributors");
    data.UsesAddressLibrary(true);
    data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1, 6, 1170, 0}});
    return data;
}();

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse)
{
    if (skse->RuntimeVersion() != REL::Version{1, 6, 1170, 0}) return false;
    auto path = SKSE::log::log_directory();
    if (!path) return false;
    *path /= "StandingOvation.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("Standing Ovation 1.1.0 beta; Skyrim 1.6.1170; constant 5x inn rewards, next-town arrival expiration.");
    const auto serialization = SKSE::GetSerializationInterface();
    serialization->SetUniqueID(signature);
    serialization->SetSaveCallback(save);
    serialization->SetLoadCallback(load);
    serialization->SetRevertCallback(revert);
    return SKSE::GetPapyrusInterface()->Register(registerPapyrus) &&
        SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
