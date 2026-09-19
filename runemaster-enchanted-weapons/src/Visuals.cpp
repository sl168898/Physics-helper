#include "Visuals.h"
#include "VisualRules.h"
#include <SKSE/SKSE.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace runes::visuals {
namespace {
// All scene graph and engine operations run on SKSE's game task queue. The
// timer only requests a refresh, with at most one outstanding task.
bool (*isEnabled)(){};
SelectAppearance selectAppearance{};
std::atomic_bool taskPending{false};
std::atomic_uint64_t generation{0};
std::uint32_t diagnosticAttachments{};

// The engine owns this controller together with its one reference effect.
// It binds to the actual weapon model, not the actor's whole skeleton. Like
// native active-effect controllers, it is deliberately not saved separately:
// visual state is reconstructed from equipped weapons and surviving rune buffs.
class WeaponController final : public RE::ReferenceEffectController {
public:
    WeaponController(RE::Actor* actor, RE::NiAVObject* model, Appearance appearance)
        : actor_(actor->GetHandle()), model_(model), appearance_(appearance) {}
    RE::TESObjectREFR* GetTargetReference() override { return actor_.get().get(); }
    RE::BGSArtObject* GetHitEffectArt() override { return appearance_.art; }
    RE::TESEffectShader* GetHitEffectShader() override { return appearance_.shader; }
    bool GetManagerHandlesSaveLoad() override { return false; }
    RE::NiAVObject* GetAttachRoot() override { return model_.get(); }
    bool GetIsRootActor() override { return false; }
    bool GetUseSourcePosition() override { return false; }
    bool GetClearWhenCellIsUnloaded() override { return true; }
    bool GetAllowTargetRoot() override { return false; }
    bool IsReadyForAttach() override {
        auto actor = actor_.get();
        return actor && actor->Is3DLoaded() && model_ && model_->parent;
    }
private:
    RE::ActorHandle actor_;
    RE::NiPointer<RE::NiAVObject> model_;
    Appearance appearance_;
};

RE::NiPointer<RE::ReferenceEffect> start(RE::Actor* actor, RE::NiAVObject* node,
    RE::TESObjectWEAP* weapon, Appearance appearance) {
    auto controller = new WeaponController(actor, node, appearance);
    RE::ReferenceEffect* effect = nullptr;
    controller->Start(&effect);
    if (!effect) {
        delete controller;
        return {};
    }
    effect->ownController = true;
    effect->lifetime = -1.f;
    if (auto shader = effect->As<RE::ShaderReferenceEffect>()) shader->wornObject = weapon;
    return RE::NiPointer<RE::ReferenceEffect>(effect);
}

struct Attachment {
    RE::NiPointer<RE::NiAVObject> node;
    RE::FormID weapon{};
    Appearance appearance;
    RE::NiPointer<RE::ReferenceEffect> art, shader;
    bool seen{};
    bool live() const {
        return (!appearance.art || (art && !art->finished)) &&
            (!appearance.shader || (shader && !shader->finished));
    }
    void stop() {
        // Finish ONLY our own effects; ordinary enchantment shaders and effects
        // from other mods are never stopped by form ID or by target actor.
        if (art) art->finished = true;
        if (shader) shader->finished = true;
        art.reset(); shader.reset(); node.reset();
    }
};
struct ActorVisuals {
    RE::ActorHandle actor;
    std::vector<Attachment> attachments;
    bool seen{};
};
std::unordered_map<RE::FormID, ActorVisuals> actors;
// Stop the timer before destroying the state that its queued work accesses.
std::jthread timer;

bool belongsTo(RE::NiAVObject* model, RE::NiAVObject* root) {
    for (unsigned depth = 0; model && depth < 128; ++depth, model = model->parent)
        if (model == root) return true;
    return false;
}

void updateActor(RE::Actor* actor, bool firstPerson) {
    if (!actor || !eligible(actor->Is3DLoaded(), !actor->IsDead(), actor->AsActorState()->IsWeaponDrawn(), true, false, false)) return;
    const auto& biped = actor->GetBiped(firstPerson);
    if (!biped || !biped->root) return;
    auto right = actor->GetEquippedObject(false);
    auto left = actor->GetEquippedObject(true);
    for (auto& part : biped->objects) {
        // Iterating the live biped also covers the off-hand model when two
        // identical weapon forms are equipped. Ignore armor and display copies.
        if (!part.item || (part.item != right && part.item != left) || !part.partClone) continue;
        auto weapon = part.item->As<RE::TESObjectWEAP>();
        if (!weapon || weapon->IsStaff() || weapon->IsHandToHandMelee()) continue;
        auto node = part.partClone.get();
        if (!belongsTo(node, biped->root)) continue;
        const auto appearance = selectAppearance(actor, weapon);
        if (!appearance) continue;
        auto& state = actors[actor->GetFormID()];
        state.actor = actor->GetHandle(); state.seen = true;
        auto found = std::find_if(state.attachments.begin(), state.attachments.end(),
            [node](const auto& attachment) { return attachment.node.get() == node; });
        if (found != state.attachments.end()) {
            if (reuse(reinterpret_cast<std::uintptr_t>(found->node.get()), reinterpret_cast<std::uintptr_t>(node),
                found->weapon, weapon->GetFormID(), found->appearance == appearance, found->live())) {
                found->seen = true;
                continue;
            }
            found->stop();
            state.attachments.erase(found);
        }
        Attachment attachment;
        attachment.node = RE::NiPointer<RE::NiAVObject>(node);
        attachment.weapon = weapon->GetFormID();
        attachment.appearance = appearance;
        attachment.seen = true;
        // Separate controllers guarantee one owner per engine effect.
        if (appearance.art) attachment.art = start(actor, node, weapon, {appearance.art, nullptr});
        if (appearance.shader) attachment.shader = start(actor, node, weapon, {nullptr, appearance.shader});
        state.attachments.push_back(std::move(attachment));
        if (diagnosticAttachments++ < 16) SKSE::log::info(
            "Rune visuals: actor={:08X}; weapon={:08X}; first-person={}; art={:08X}; shader={:08X}",
            actor->GetFormID(), weapon->GetFormID(), firstPerson,
            appearance.art ? appearance.art->GetFormID() : 0,
            appearance.shader ? appearance.shader->GetFormID() : 0);
    }
}

void refresh(std::uint64_t epoch) {
    taskPending = false;
    if (generation != epoch || !isEnabled || !isEnabled()) return;
    const auto ui = RE::UI::GetSingleton();
    if (ui && ui->GameIsPaused()) return;
    for (auto& [id, state] : actors) {
        state.seen = false;
        for (auto& attachment : state.attachments) attachment.seen = false;
    }
    auto player = RE::PlayerCharacter::GetSingleton();
    const auto camera = RE::PlayerCamera::GetSingleton();
    updateActor(player, camera && camera->IsInFirstPerson());
    if (auto processes = RE::ProcessLists::GetSingleton()) {
        processes->ForEachHighActor([player](RE::Actor* actor) {
            if (actor != player) updateActor(actor, false);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }
    for (auto it = actors.begin(); it != actors.end();) {
        auto& state = it->second;
        for (auto part = state.attachments.begin(); part != state.attachments.end();) {
            if (!state.seen || !part->seen) { part->stop(); part = state.attachments.erase(part); }
            else ++part;
        }
        if (state.attachments.empty()) it = actors.erase(it);
        else ++it;
    }
}
}

void install(bool (*enabled)(), SelectAppearance select) {
    if (timer.joinable()) return;
    isEnabled = enabled;
    selectAppearance = select;
    timer = std::jthread([](std::stop_token stop) {
        std::mutex waitMutex;
        std::condition_variable_any wake;
        std::unique_lock lock(waitMutex);
        while (!stop.stop_requested()) {
            wake.wait_for(lock, stop, std::chrono::milliseconds(250), [] { return false; });
            if (stop.stop_requested()) break;
            if (!isEnabled || !isEnabled() || taskPending.exchange(true)) continue;
            const auto epoch = generation.load();
            SKSE::GetTaskInterface()->AddTask([epoch] { refresh(epoch); });
        }
    });
    SKSE::log::info("Rune weapon visuals enabled: original art/shaders, separate from inventory enchantments");
}
void reset() {
    ++generation;
    for (auto& [id, state] : actors)
        for (auto& attachment : state.attachments) attachment.stop();
    actors.clear();
    diagnosticAttachments = 0;
}
}
