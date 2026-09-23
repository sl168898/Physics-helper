#pragma once
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>

namespace traits {
// Vanilla Game reward functions, sequenced by VM completion callbacks. Results
// return to the game thread. No added script, soul charge or quest-stage change.
class IronLungsGrant {
    struct Callback final : RE::BSScript::IStackCallbackFunctor {
        std::function<void(bool)> finish;
        explicit Callback(std::function<void(bool)> f) : finish(std::move(f)) {}
        void operator()(RE::BSScript::Variable value) override {
            auto f = finish;
            const bool result = value.IsBool() && value.GetBool();
            SKSE::GetTaskInterface()->AddTask([f = std::move(f), result] { f(result); });
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    };
    std::recursive_mutex mutex;
    RE::TESShout* shout = nullptr;
    std::function<bool()> selected;
    std::uint64_t epoch = 0;
    bool hadTrait = false, busy = false, complete = false, changed = false;
    float retryAfter = 0, pendingFor = 0;

    bool current(std::uint64_t generation) const {
        auto p = RE::PlayerCharacter::GetSingleton();
        return generation == epoch && hadTrait && selected && selected() && p && !p->IsDead();
    }
    static bool call(const char* name, RE::TESWordOfPower* word, std::function<void(bool)> finish) {
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!vm || !word) return false;
        std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(std::move(word)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback = RE::make_smart<Callback>(std::move(finish));
        return args && vm->DispatchStaticCall("Game", name, args.get(), callback);
    }
    void failed(std::uint64_t generation) {
        if (!current(generation)) return;
        ++epoch; busy = false; retryAfter = 5.f; pendingFor = 0;
        SKSE::log::warn("[IronLungs] Word grant incomplete; retrying after five gameplay seconds");
    }
    void unlock(unsigned index, std::uint64_t generation) {
        if (!current(generation)) return;
        auto word = shout->variations[index].word;
        if (!word->GetKnown()) { failed(generation); return; }
        // UnlockWord does not charge souls. Already unlocked words skip it.
        if (!call("UnlockWord", word, [this, word, index, generation](bool) {
            std::lock_guard lock(mutex);
            if (!current(generation)) return;
            if (!call("IsWordUnlocked", word, [this, word, index, generation](bool unlocked) {
                std::lock_guard verifyLock(mutex);
                if (!current(generation)) return;
                if (!unlocked || !word->GetKnown()) { failed(generation); return; }
                changed = true;
                SKSE::log::info("[IronLungs] Granted word {}: {:08X}", index + 1, word->GetFormID());
                next(index + 1, generation);
            })) failed(generation);
        })) failed(generation);
    }
    void next(unsigned index, std::uint64_t generation) {
        if (!current(generation)) return;
        pendingFor = 0;
        if (index == 3) {
            complete = true; busy = false;
            SKSE::log::info("[IronLungs] All three Unrelenting Force words verified unlocked; changed={}", changed);
            if (changed) RE::DebugNotification("Iron Lungs: all three words of Unrelenting Force unlocked.");
            return;
        }
        auto word = shout->variations[index].word;
        if (!call("IsWordUnlocked", word, [this, word, index, generation](bool unlocked) {
            std::lock_guard lock(mutex);
            if (!current(generation)) return;
            if (unlocked && word->GetKnown()) { next(index + 1, generation); return; }
            if (word->GetKnown()) { unlock(index, generation); return; }
            if (!call("TeachWord", word, [this, index, generation](bool) {
                std::lock_guard teachLock(mutex);
                if (current(generation)) unlock(index, generation);
            })) failed(generation);
        })) failed(generation);
    }
public:
    void init(RE::TESShout* value, std::function<bool()> enabled) {
        shout = value; selected = std::move(enabled);
    }
    void reset() {
        std::lock_guard lock(mutex);
        ++epoch; hadTrait = busy = complete = changed = false; retryAfter = pendingFor = 0;
    }
    void tick(float dt) {
        std::lock_guard lock(mutex);
        auto p = RE::PlayerCharacter::GetSingleton();
        const bool enabled = selected && selected() && p && !p->IsDead();
        if (enabled != hadTrait) {
            ++epoch; hadTrait = enabled; busy = complete = changed = false; retryAfter = pendingFor = 0;
        }
        if (!enabled || !shout || complete) return;
        const float elapsed = std::isfinite(dt) && dt > 0 ? dt : 0.f;
        if (busy) {
            pendingFor += elapsed;
            if (pendingFor >= 30.f) failed(epoch);
            return;
        }
        retryAfter -= elapsed;
        if (retryAfter > 0) return;
        for (const auto& variation : shout->variations) {
            if (!variation.word) { failed(epoch); return; }
        }
        if (!p->HasShout(shout)) {
            p->AddShout(shout);
            if (!p->HasShout(shout)) { failed(epoch); return; }
            changed = true;
        }
        busy = true;
        next(0, epoch);
    }
};
}
