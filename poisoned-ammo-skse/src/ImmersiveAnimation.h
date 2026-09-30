#pragma once

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include "ReloadGate.h"

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
        Guard presentationReady;
        bool trace;
        std::atomic_bool used = false;
    public:
        StatUpdated(RE::AlchemyItem* poison, std::int32_t count, Guard valid, bool tracing, Guard ready) :
            bottles(count), current(std::move(valid)), presentationReady(std::move(ready)), trace(tracing)
        {
            // A Papyrus variable retains the form handle, including the last
            // bottle of a player-created poison, across this asynchronous call.
            poisonForm.Pack(static_cast<RE::TESForm*>(poison));
        }
        void SetObject(const Script&) override {}
        void operator()(RE::BSScript::Variable) override
        {
            if (used.exchange(true) || !current()) return;
            SKSE::GetTaskInterface()->AddTask([form = poisonForm, count = bottles, valid = current, tracing = trace, ready = presentationReady] {
                if (!valid() || (ready && !ready())) return;
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

    inline void dispatch(RE::AlchemyItem* poison, std::int32_t bottles, Guard current, bool trace, Guard presentationReady = {})
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
            RE::make_smart<StatUpdated>(poison, bottles, std::move(current), trace, std::move(presentationReady));
        if (!vm->DispatchStaticCall(RE::BSFixedString("Game"), RE::BSFixedString("IncrementStat"), args.get(), callback))
            SKSE::log::warn("Immersive Interactions animation skipped: could not update Poisons Used");
    }

    struct EquipmentContext
    {
        RE::FormID weapon{}, originalAmmo{}, expectedAmmo{};
        std::uint64_t reloadCheckpoint{};
        bool drawnCrossbow = false;
    };
    inline std::atomic<std::uint64_t> reloadSignal = 0, requestSerial = 0;

    inline EquipmentContext capture(RE::PlayerCharacter* player, RE::TESAmmo* original, RE::TESAmmo* expected)
    {
        EquipmentContext result;
        result.reloadCheckpoint = reloadSignal.load();
        result.originalAmmo = original ? original->GetFormID() : 0;
        result.expectedAmmo = expected ? expected->GetFormID() : 0;
        auto form = player ? player->GetEquippedObject(false) : nullptr;
        auto weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
        result.weapon = weapon ? weapon->GetFormID() : 0;
        result.drawnCrossbow = weapon && weapon->IsCrossbow() && player->IsWeaponDrawn();
        return result;
    }

    struct Pending
    {
        RE::BSScript::Variable poisonForm;
        std::int32_t bottles;
        Guard current;
        EquipmentContext equipment;
        ReloadGate gate;
        bool trace, ammoMatched = false;
        Pending(RE::AlchemyItem* poison, std::int32_t count, Guard valid, bool tracing, EquipmentContext context) :
            bottles(count), current(std::move(valid)), equipment(context), gate(context.reloadCheckpoint), trace(tracing)
        {
            poisonForm.Pack(static_cast<RE::TESForm*>(poison));
        }
    };
    // Accessed only by crafting/game-thread updates. Graph callbacks only write
    // reloadSignal; they never access this object, the VM, or inventory.
    inline std::unique_ptr<Pending> pending;

    inline void completed(RE::AlchemyItem* poison, std::int32_t bottles, Guard current, bool trace,
        EquipmentContext context)
    {
        if (!poison || bottles <= 0 || !current()) return;
        const auto serial = ++requestSerial;
        pending.reset(); // A newer batch replaces an obsolete visual request.
        Guard valid = [current = std::move(current), serial] { return current() && requestSerial == serial; };
        if (!context.drawnCrossbow) {
            dispatch(poison, bottles, std::move(valid), trace);
            return;
        }
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if (!playerAlias(vm)) return;
        pending = std::make_unique<Pending>(poison, bottles, std::move(valid), trace, context);
        if (trace) SKSE::log::info("Crossbow poison animation queued: waiting for reload; bottles {}", bottles);
    }

    inline void update(RE::PlayerCharacter* player, float delta)
    {
        if (!pending) return;
        auto& request = *pending;
        const auto& context = request.equipment;
        // Do not dereference forms or dispatch Papyrus after load/revert.
        if (!request.current()) { pending.reset(); return; }
        auto weapon = player ? player->GetEquippedObject(false) : nullptr;
        auto ammo = player ? player->GetCurrentAmmo() : nullptr;
        const auto ammoID = ammo ? ammo->GetFormID() : 0;
        const bool equipmentReady = ammoID == context.expectedAmmo;
        request.ammoMatched = request.ammoMatched || equipmentReady;
        // EquipObject may finish on a subsequent update. Allow its original or
        // temporarily empty ammo slot until the intended ammo first appears.
        const bool ammoValid = equipmentReady || (!request.ammoMatched &&
            (!ammoID || ammoID == context.originalAmmo));
        const bool valid = player && !player->IsDead() && player->IsWeaponDrawn() && weapon &&
            weapon->GetFormID() == context.weapon && ammoValid;
        auto ui = RE::UI::GetSingleton();
        const bool paused = !ui || ui->GameIsPaused() || ui->IsItemMenuOpen() || ui->IsModalMenuOpen();
        bool isReloading = false;
        std::optional<bool> graphReloading;
        if (valid && player->GetGraphVariableBool(RE::BSFixedString("IsReloading"), isReloading)) graphReloading = isReloading;
        const auto result = request.gate.advance(delta, valid, paused, equipmentReady, graphReloading, reloadSignal.load());
        if (result == ReloadResult::waiting) return;
        auto finished = std::move(pending); // Clear before any asynchronous dispatch; exactly once.
        if (result == ReloadResult::cancelled) {
            if (finished->trace) SKSE::log::info("Crossbow poison animation cancelled: equipment changed or reload did not settle");
            return;
        }
        auto poison = finished->poisonForm.Unpack<RE::TESForm*>();
        auto alchemy = poison ? poison->As<RE::AlchemyItem>() : nullptr;
        if (!alchemy || !finished->current()) return;
        if (finished->trace) SKSE::log::info("Crossbow reload settled: requesting poison animation");
        // Delaying IncrementStat as well as OnItemRemoved prevents the genuine
        // removal event from starting New Anims before the reload is finished.
        dispatch(alchemy, finished->bottles, std::move(finished->current), finished->trace, [context = finished->equipment] {
            // This predicate runs only in StatUpdated's game task, never in
            // the VM worker callback. Recheck across the asynchronous stat call.
            auto actor = RE::PlayerCharacter::GetSingleton();
            auto held = actor ? actor->GetEquippedObject(false) : nullptr;
            auto selected = actor ? actor->GetCurrentAmmo() : nullptr;
            auto menus = RE::UI::GetSingleton();
            bool reloading = false;
            return actor && !actor->IsDead() && actor->IsWeaponDrawn() && held && selected &&
                held->GetFormID() == context.weapon && selected->GetFormID() == context.expectedAmmo &&
                menus && !menus->GameIsPaused() && !menus->IsItemMenuOpen() && !menus->IsModalMenuOpen() &&
                !(actor->GetGraphVariableBool(RE::BSFixedString("IsReloading"), reloading) && reloading);
        });
    }

    struct UpdateHook
    {
        static void call(RE::PlayerCharacter* player, float delta)
        {
            original(player, delta);
            try { update(player, delta); }
            catch (const std::exception& e) {
                pending.reset(); ++requestSerial;
                SKSE::log::error("Crossbow animation wait failed after completed batch: {}", e.what());
            }
        }
        static inline REL::Relocation<decltype(call)> original;
    };
    struct GraphEventHook
    {
        static RE::BSEventNotifyControl call(RE::BSTEventSink<RE::BSAnimationGraphEvent>* sink,
            const RE::BSAnimationGraphEvent* event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* source)
        {
            const auto result = original(sink, event, source);
            if (event && event->holder == RE::PlayerCharacter::GetSingleton()) {
                const auto kind = reloadEventType(event->tag.c_str());
                if (kind != ReloadEvent::none) {
                    auto previous = reloadSignal.load();
                    while (!reloadSignal.compare_exchange_weak(previous, nextReloadSignal(previous, kind))) {}
                }
            }
            return result; // Observe only: never suppress or replace a graph event.
        }
        static inline REL::Relocation<decltype(call)> original;
    };
    inline void install()
    {
        // This plugin only accepts 1.6.1170. Chain the player's Update and
        // animation-event sink; no graph-sink lifetime survives a 3D rebuild.
        REL::Relocation<std::uintptr_t> player{ RE::VTABLE_PlayerCharacter[0] };
        REL::Relocation<std::uintptr_t> events{ RE::VTABLE_PlayerCharacter[2] };
        UpdateHook::original = player.write_vfunc(0xAD, UpdateHook::call);
        GraphEventHook::original = events.write_vfunc(0x01, GraphEventHook::call);
    }
}
