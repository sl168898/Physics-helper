#pragma once
#include "Skald.h"
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace traits {
// All game/UI operations run on the game task queue. VM callbacks own their
// captures and only enqueue results; no VM pumping or stack-local references.
class SkaldRuntime final : public RE::BSTEventSink<RE::TESSpellCastEvent> {
    struct BoolCallback final : RE::BSScript::IStackCallbackFunctor {
        std::function<void(bool)> finish;
        explicit BoolCallback(std::function<void(bool)> f) : finish(std::move(f)) {}
        void operator()(RE::BSScript::Variable value) override {
            auto f = finish;
            const bool answer = value.IsBool() && value.GetBool();
            SKSE::GetTaskInterface()->AddTask([f = std::move(f), answer] { f(answer); });
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };
    struct ChoiceCallback final : RE::IMessageBoxCallback {
        std::function<void(unsigned)> finish;
        explicit ChoiceCallback(std::function<void(unsigned)> f) : finish(std::move(f)) { unk0C = 0; }
        void Run(Message value) override {
            auto f = finish;
            SKSE::GetTaskInterface()->AddTask([f = std::move(f), value] { f(static_cast<unsigned>(value)); });
        }
    };
    struct Choice { RE::FormID id; std::string name; };
    struct Batch { std::vector<Choice> choices; std::size_t pending = 0; };
    std::recursive_mutex mutex;
    Skald state;
    RE::SpellItem *trait = nullptr, *power = nullptr;
    bool running = false, syncPending = true, hadTrait = false, valid = false;
    bool menuBusy = false, inCast = false;
    std::uint64_t epoch = 0, menuTicket = 0;
    static constexpr std::uint32_t recordID = 0x534B4C44; // SKLD

    bool active() const {
        auto p = RE::PlayerCharacter::GetSingleton();
        return running && p && trait && power && p->HasSpell(trait);
    }
    static RE::TESShout* known(RE::FormID id) {
        auto p = RE::PlayerCharacter::GetSingleton();
        auto s = RE::TESForm::LookupByID<RE::TESShout>(id);
        if (!p || !s || !p->HasShout(s)) return nullptr;
        const auto& first = s->variations[0];
        return first.word && first.word->GetKnown() && first.spell ? s : nullptr;
    }
    static void unlocked(RE::FormID id, std::function<void(bool)> finish) {
        auto shout = known(id);
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!shout || !vm) { finish(false); return; }
        auto word = shout->variations[0].word;
        std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(std::move(word)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback = RE::make_smart<BoolCallback>(finish);
        if (!args || !vm->DispatchStaticCall("Game", "IsWordUnlocked", args.get(), callback)) finish(false);
    }
    void validateStored() {
        valid = false;
        if (!state.shout) return;
        const auto id = state.shout;
        const auto generation = epoch;
        unlocked(id, [this, id, generation](bool answer) {
            std::lock_guard lock(mutex);
            if (epoch != generation || state.shout != id || !active()) return;
            valid = answer && known(id);
            if (!valid) SKSE::log::warn("Skald: saved shout {:08X} is missing, unlearned or locked", id);
        });
    }
    bool show(const std::string& body, const std::vector<std::string>& buttons,
        std::function<void(unsigned)> finish) {
        auto factories = RE::MessageDataFactoryManager::GetSingleton();
        auto strings = RE::InterfaceStrings::GetSingleton();
        auto factory = factories && strings ? factories->GetCreator<RE::MessageBoxData>(strings->messageBoxData) : nullptr;
        if (!factory) return false;
        auto box = factory->Create();
        if (!box) return false;
        box->unk4C = 4;
        box->unk38 = 10;
        box->bodyText = body.c_str();
        for (const auto& label : buttons) box->buttonText.push_back(label.c_str());
        box->callback = RE::make_smart<ChoiceCallback>(std::move(finish));
        box->QueueMessage();
        return true;
    }
    void page(std::shared_ptr<Batch> batch, std::size_t offset, std::uint64_t generation, std::uint64_t ticket) {
        if (epoch != generation || menuTicket != ticket || !active()) return;
        constexpr std::size_t pageSize = 5; // at most 9 buttons, including navigation
        const auto count = std::min(pageSize, batch->choices.size() - offset);
        std::vector<std::string> labels;
        std::vector<int> actions;
        for (std::size_t i = 0; i < count; ++i) {
            labels.push_back(batch->choices[offset + i].name);
            actions.push_back(static_cast<int>(offset + i));
        }
        if (offset) { labels.emplace_back("Previous"); actions.push_back(-1); }
        if (offset + count < batch->choices.size()) { labels.emplace_back("Next"); actions.push_back(-2); }
        labels.emplace_back("Clear stored shout"); actions.push_back(-3);
        labels.emplace_back("Cancel"); actions.push_back(-4);
        auto current = known(state.shout);
        const std::string body = std::string("Skald - Store Shout\nCurrent: ") +
            (current ? current->GetName() : "None") + "\nChoose an unlocked shout. Power attacks release its first word." +
            (batch->choices.empty() ? "\nNo unlocked shouts are available." : "");
        if (!show(body, labels, [this, batch, offset, generation, ticket, actions](unsigned button) {
            std::lock_guard lock(mutex);
            if (epoch != generation || menuTicket != ticket || !active()) return;
            if (button >= actions.size()) { menuBusy = false; return; }
            const int action = actions[button];
            if (action == -1) { page(batch, offset - pageSize, generation, ticket); return; }
            if (action == -2) { page(batch, offset + pageSize, generation, ticket); return; }
            if (action == -3) {
                state.choose(0); valid = false; menuBusy = false;
                RE::DebugNotification("Skald: stored shout cleared.");
            } else if (action >= 0) {
                auto choice = batch->choices[static_cast<std::size_t>(action)];
                unlocked(choice.id, [this, choice, generation, ticket](bool answer) {
                    std::lock_guard choiceLock(mutex);
                    if (epoch != generation || menuTicket != ticket || !active()) return;
                    menuBusy = false;
                    if (!answer || !known(choice.id)) { RE::DebugNotification("Skald: that shout is not unlocked."); return; }
                    state.choose(choice.id); valid = true;
                    RE::DebugNotification(("Skald stored: " + choice.name).c_str());
                    SKSE::log::info("Skald stored {} ({:08X})", choice.name, choice.id);
                });
            } else menuBusy = false;
        })) { menuBusy = false; SKSE::log::error("Skald: message box factory unavailable"); }
    }
    void chooser() {
        std::lock_guard lock(mutex);
        if (!active() || menuBusy) return;
        menuBusy = true;
        const auto ticket = ++menuTicket, generation = epoch;
        auto batch = std::make_shared<Batch>();
        std::vector<Choice> candidates;
        auto data = RE::TESDataHandler::GetSingleton();
        for (auto shout : data->GetFormArray<RE::TESShout>()) {
            if (!shout || !known(shout->GetFormID()) || !shout->GetName()[0]) continue;
            candidates.push_back({shout->GetFormID(), shout->GetName()});
        }
        batch->pending = candidates.size();
        if (!batch->pending) { page(batch, 0, generation, ticket); return; }
        for (const auto& choice : candidates) unlocked(choice.id, [this, batch, choice, generation, ticket](bool answer) {
            std::lock_guard resultLock(mutex);
            if (epoch != generation || menuTicket != ticket || !active()) return;
            if (answer) batch->choices.push_back(choice);
            if (--batch->pending == 0) {
                std::sort(batch->choices.begin(), batch->choices.end(), [](const Choice& a, const Choice& b) {
                    return a.name == b.name ? a.id < b.id : a.name < b.name;
                });
                page(batch, 0, generation, ticket);
            }
        });
    }
public:
    void init(RE::TESDataHandler* data, const char* file) {
        trait = data->LookupForm<RE::SpellItem>(0xB00, file);
        power = data->LookupForm<RE::SpellItem>(0xB06, file);
        if (trait && power) {
            RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSpellCastEvent>(this);
            SKSE::log::info("Skald ready: stored first word, 10/6/3 seconds at base Speech 0/50/100");
        } else SKSE::log::error("Skald v2.6 forms missing; Skald disabled");
    }
    void resetTransient(bool enable) {
        std::lock_guard lock(mutex);
        ++epoch; ++menuTicket; running = enable;
        syncPending = true; hadTrait = false; valid = false; menuBusy = false; inCast = false;
    }
    void clear() { std::lock_guard lock(mutex); state = {}; resetTransient(false); }
    bool casting() const { return inCast; }
    void tick(RE::PlayerCharacter* p, float dt) {
        std::lock_guard lock(mutex);
        state.tick(dt);
        const bool enabled = active();
        if (syncPending || enabled != hadTrait) {
            ++epoch; ++menuTicket; menuBusy = false;
            if (enabled) {
                // Existing saves can contain the retired cloak instance.
                // Reapply this ability to attach the neutral marker and keep
                // exactly one copy of the unchanged light-attack penalty.
                p->RemoveSpell(trait);
                p->AddSpell(trait);
                if (!p->HasSpell(power)) p->AddSpell(power);
                validateStored();
            } else {
                if (power && p->HasSpell(power)) p->RemoveSpell(power);
                valid = false;
            }
            hadTrait = enabled; syncPending = false;
        }
    }
    bool attack(RE::PlayerCharacter* p, bool bash, bool isPower, bool melee) {
        std::lock_guard lock(mutex);
        auto ui = RE::UI::GetSingleton();
        if (inCast || !active() || !valid || p->IsDead() || p->IsInKillMove() || (ui && ui->GameIsPaused())) return false;
        auto shout = known(state.shout);
        if (!shout) { valid = false; return false; }
        auto spell = shout->variations[0].spell;
        auto caster = p->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
        if (!caster || spell->GetCastingType() != RE::MagicSystem::CastingType::kFireAndForget) return false;
        RE::NiPointer<RE::TESObjectREFR> target;
        using Delivery = RE::MagicSystem::Delivery;
        if (spell->GetDelivery() == Delivery::kSelf) target.reset(p);
        else if (spell->GetDelivery() != Delivery::kAimed) {
            auto pick = RE::CrosshairPickData::GetSingleton();
            if (pick) target = pick->targetActor.get();
            if (!target) return false;
        }
        if (!state.start(p->AsActorValueOwner()->GetBaseActorValue(RE::ActorValue::kSpeech), true, true, bash, isPower, melee)) return false;
        inCast = true;
        // The exact first-word spell from the loaded shout (including overrides).
        // No synthetic VoiceFire event and no edits to normal shout recovery.
        caster->CastSpellImmediate(spell, true, target.get(), 1.f, false, 0.f, p);
        inCast = false;
        SKSE::log::info("Skald released {} first word; recovery {} seconds", shout->GetName(), state.remaining);
        return true;
    }
    RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override {
        if (event && power && event->spell == power->GetFormID() && event->object.get() == RE::PlayerCharacter::GetSingleton()) {
            std::lock_guard lock(mutex);
            const auto generation = epoch;
            SKSE::GetTaskInterface()->AddTask([this, generation] {
                std::lock_guard taskLock(mutex);
                if (generation == epoch) chooser();
            });
        }
        return RE::BSEventNotifyControl::kContinue;
    }
    void save(SKSE::SerializationInterface* api) {
        std::lock_guard lock(mutex);
        if (!api->OpenRecord(recordID, 1) || !api->WriteRecordData(&state.shout, 4) || !api->WriteRecordData(&state.remaining, 4))
            SKSE::log::error("Skald: failed to write co-save state");
    }
    void load(SKSE::SerializationInterface* api) {
        std::lock_guard lock(mutex);
        clear();
        std::uint32_t type = 0, version = 0, length = 0;
        while (api->GetNextRecordInfo(type, version, length)) {
            if (type != recordID || version != 1 || length != 8) continue;
            RE::FormID old = 0, resolved = 0; float recovery = 0;
            if (api->ReadRecordData(&old, 4) != 4 || api->ReadRecordData(&recovery, 4) != 4) continue;
            if (old) api->ResolveFormID(old, resolved);
            state.restore(resolved, recovery);
            SKSE::log::info("Skald restored shout {:08X}; remaining {}", resolved, state.remaining);
        }
    }
};
}
