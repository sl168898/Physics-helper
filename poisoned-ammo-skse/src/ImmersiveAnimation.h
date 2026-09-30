#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <functional>
#include <memory>

namespace pa::animation
{
    using Guard = std::function<bool()>;
    using Script = RE::BSTSmartPointer<RE::BSScript::Object>;

    // Resolve the installed player's alias, never an invented quest FormID or
    // an arbitrary script with the same event. Re-resolve after asynchronous work.
    inline Script playerAlias(RE::BSScript::IVirtualMachine* vm)
    {
        auto data = RE::TESDataHandler::GetSingleton();
        auto player = RE::PlayerCharacter::GetSingleton();
        auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!data || !player || !policy || !data->LookupModByName("ImmersiveInteractions.esp")) return {};
        for (auto quest : data->GetFormArray<RE::TESQuest>()) {
            const auto file = quest ? quest->GetFile(0) : nullptr;
            if (!file || _stricmp(file->GetFilename().data(), "ImmersiveInteractions.esp") || !quest->IsRunning()) continue;
            for (auto base : quest->aliases) {
                if (!base || base->GetVMTypeID() != RE::BGSRefAlias::VMTYPEID) continue;
                auto alias = static_cast<RE::BGSRefAlias*>(base);
                if (alias->GetReference() != player) continue;
                const auto handle = policy->GetHandleForObject(RE::BGSRefAlias::VMTYPEID, alias);
                Script script;
                if (handle != policy->EmptyHandle() && vm->FindBoundObject(handle, "AR_Ref_AliasScript", script) && script)
                    return script;
            }
        }
        return {};
    }

    class StatUpdated final : public RE::BSScript::IStackCallbackFunctor
    {
        RE::BSScript::Variable poisonForm;
        std::int32_t bottles;
        Guard current;
        bool trace;
        std::atomic_bool used = false;
    public:
        StatUpdated(RE::AlchemyItem* poison, std::int32_t count, Guard valid, bool tracing) :
            bottles(count), current(std::move(valid)), trace(tracing)
        {
            // A Papyrus variable retains the form handle, including the last
            // bottle of a player-created poison, across this asynchronous call.
            poisonForm.Pack(static_cast<RE::TESForm*>(poison));
        }
        void SetObject(const Script&) override {}
        void operator()(RE::BSScript::Variable) override
        {
            if (used.exchange(true) || !current()) return;
            SKSE::GetTaskInterface()->AddTask([form = poisonForm, count = bottles, valid = current, tracing = trace] {
                if (!valid()) return;
                try {
                    auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
                    auto script = playerAlias(vm);
                    auto poison = form.Unpack<RE::TESForm*>();
                    if (!script || !poison) return;
                    // New Anims 1.5 handles OnItemRemoved + the increased
                    // "Poisons Used" stat. The real removal event may already
                    // have run; its previousStat/AR_DogUp checks suppress a
                    // second animation. Target only this script, not the player
                    // or all subscribers, and never fake OnObjectPoisoned.
                    std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(
                        static_cast<RE::TESForm*>(poison), std::int32_t(count),
                        static_cast<RE::TESObjectREFR*>(nullptr), static_cast<RE::TESObjectREFR*>(nullptr)));
                    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
                    const bool sent = vm->DispatchMethodCall(script, RE::BSFixedString("OnItemRemoved"), args.get(), callback);
                    if (tracing || !sent) SKSE::log::info("Immersive Interactions completed-batch animation request: {}; bottles {}", sent, count);
                } catch (const std::exception& e) {
                    SKSE::log::error("Immersive Interactions animation request failed: {}", e.what());
                }
            });
        }
    };

    inline void completed(RE::AlchemyItem* poison, std::int32_t bottles, Guard current, bool trace)
    {
        if (!poison || bottles <= 0 || !current()) return;
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!playerAlias(vm)) {
            if (trace) SKSE::log::info("Immersive Interactions animation skipped: player alias is not active");
            return;
        }
        // Only committed transactions reach here. Count the bottles actually
        // used, then ask the installed script to handle its own animation,
        // camera, menu, drawn-weapon, busy-state and ignore-keyword conditions.
        // No third-party script or animation asset is replaced.
        std::unique_ptr<RE::BSScript::IFunctionArguments> args(RE::MakeFunctionArguments(
            RE::BSFixedString("Poisons Used"), std::int32_t(bottles)));
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback =
            RE::make_smart<StatUpdated>(poison, bottles, std::move(current), trace);
        if (!vm->DispatchStaticCall(RE::BSFixedString("Game"), RE::BSFixedString("IncrementStat"), args.get(), callback))
            SKSE::log::warn("Immersive Interactions animation skipped: could not update Poisons Used");
    }
}
