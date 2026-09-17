#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include "SKSEMenuFramework.h"
#include "Settings.h"
#include "Keys.h"
#include "ProfileStore.h"

namespace
{
    namespace imgui = ImGuiMCP;
    using Clock = std::chrono::steady_clock;
    constexpr auto originalPlugin = "The White Phial - Tweaks and Enhancements.esp";
    std::array<RE::TESGlobal*, phial::count> globals{};
    bool formsReady = false;  // Only read/written by game thread tasks and SKSE messages.
    bool registered = false;
    RE::EffectSetting* hotkeyEffect = nullptr;
    const std::filesystem::path profilePath = "Data/SKSE/Plugins/WhitePhialMenu.ini";

    struct Shared
    {
        std::uint64_t epoch = 1;
        bool session = false;
        bool ready = false;
        bool pollPending = false;
        bool applyPending = false;
        bool remember = false;
        bool profileRead = false;
        bool restorePending = false;
        bool saveFailed = false;
        bool configError = false;
        phial::Profile profile{};
        phial::Values values{};
        std::string status = "Load a save to edit the White Phial settings.";
        bool error = false;
        std::string hotkeyStatus;
    } shared;
    std::mutex sharedMutex;

    bool inGame()
    {
        auto ui = RE::UI::GetSingleton();
        return ui && RE::PlayerCharacter::GetSingleton() &&
            !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
    }

    phial::Values readValues()
    {
        phial::Values values{};
        for (std::size_t i = 0; i < phial::count; ++i) values[i] = globals[i]->value;
        return values;
    }

    void resetSession(bool active)
    {
        std::lock_guard lock(sharedMutex);
        ++shared.epoch;
        shared.session = active;
        shared.ready = shared.pollPending = shared.applyPending = false;
        shared.remember = shared.profileRead = shared.restorePending = false;
        shared.saveFailed = shared.configError = false;
        shared.profile = {};
        shared.hotkeyStatus.clear();
        shared.error = active && !formsReady;
        shared.status = !active ? "Load a save to edit the White Phial settings." :
            formsReady ? "Reading the current save..." :
            "The White Phial settings were not found. Enable White Phial - Tweaks and Enhancements (2.1 or later).";
    }

    class KeyRegistrationComplete final : public RE::BSScript::IStackCallbackFunctor
    {
    public:
        KeyRegistrationComplete(std::uint64_t epoch, std::int32_t key) : epoch_(epoch), key_(key) {}
        void operator()(RE::BSScript::Variable) override
        {
            std::lock_guard lock(sharedMutex);
            if (shared.epoch != epoch_ || !shared.session ||
                shared.values[phial::hotkey] != static_cast<float>(key_)) return;
            shared.hotkeyStatus = "Hotkey registration refreshed. Close the menu before using the phial.";
            SKSE::log::info("Hotkey RegisterForKey completed: code={}; session={}", key_, epoch_);
        }
        void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
    private:
        std::uint64_t epoch_;
        std::int32_t key_;
    };

    void refreshHotkeyRegistration(std::uint64_t epoch)
    {
        // Called only on the game thread. Reset deduplication after every load:
        // the same VM handle/key may have different registrations in another save.
        static std::uint64_t cachedEpoch = 0;
        static std::unordered_map<RE::VMHandle, std::int32_t> queued;
        if (cachedEpoch != epoch) { queued.clear(); cachedEpoch = epoch; }
        if (!formsReady || !inGame() || !phial::valid(phial::hotkey, globals[phial::hotkey]->value)) return;
        auto player = RE::PlayerCharacter::GetSingleton();
        auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        auto target = player ? player->AsMagicTarget() : nullptr;
        auto effects = target ? target->GetActiveEffectList() : nullptr;
        std::string status = "Waiting for the original White Phial hotkey effect.";
        if (hotkeyEffect && policy && effects) {
            const auto key = static_cast<std::int32_t>(globals[phial::hotkey]->value);
            bool found = false, changed = false, failed = false;
            for (auto effect : *effects) {
                if (!effect || effect->GetBaseObject() != hotkeyEffect ||
                    effect->flags.any(RE::ActiveEffect::Flag::kDispelled)) continue;
                const auto handle = policy->GetHandleForObject(RE::ActiveEffect::VMTYPEID, effect);
                if (handle == policy->EmptyHandle()) continue;
                RE::BSTSmartPointer<RE::BSScript::Object> object;
                if (!vm->FindBoundObject(handle, "TWPTPE_Hotkey_Script", object) || !object) continue;
                found = true;
                if (const auto it = queued.find(handle); it != queued.end() && it->second == key) continue;
                // Invoke the inherited SKSE method on this exact existing script.
                // Keep old registrations: its OnKeyDown rejects every key except
                // the current global. This avoids unregister/register races and
                // never touches another mod's registrations or restarts a spell.
                RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{
                    new KeyRegistrationComplete(epoch, key) };
                auto keyArgument = key;
                const bool accepted = vm->DispatchMethodCall(object, "RegisterForKey",
                    RE::MakeFunctionArguments(keyArgument), callback);
                SKSE::log::info("Hotkey RegisterForKey queued: code={}; handle={:X}; accepted={}", key, handle, accepted);
                if (accepted) { queued[handle] = key; changed = true; }
                else failed = true;
            }
            if (failed) status = "Hotkey registration could not be queued. Check WhitePhialMenu.log.";
            else if (changed) status = "Hotkey registration queued. Close the menu to let the script finish.";
            else if (found) return;  // Preserve callback completion feedback.
        }
        std::lock_guard lock(sharedMutex);
        if (epoch == shared.epoch && shared.session) shared.hotkeyStatus = std::move(status);
    }

    bool writeSetting(std::size_t field, float value);

    void requestRefresh()
    {
        auto tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        std::uint64_t epoch;
        {
            std::lock_guard lock(sharedMutex);
            if (!shared.session || shared.pollPending || shared.applyPending) return;
            shared.pollPending = true;
            epoch = shared.epoch;
        }
        tasks->AddTask([epoch] {
            bool needsProfile;
            {
                std::lock_guard lock(sharedMutex);
                if (epoch != shared.epoch || !shared.session) return;
                needsProfile = !shared.profileRead;
            }
            if (needsProfile) {
                const auto loaded = phial::loadProfile(profilePath);
                std::lock_guard lock(sharedMutex);
                if (epoch != shared.epoch || !shared.session) return;
                shared.profileRead = true;
                shared.profile = loaded.profile;
                shared.remember = loaded && loaded.profile.enabled;
                shared.restorePending = shared.remember;
                if (!loaded) {
                    shared.error = shared.configError = true;
                    shared.status = "Shared settings could not be read: " + loaded.error + " Choose settings and Apply to replace the configuration.";
                    SKSE::log::error("Shared configuration: {}", loaded.error);
                } else SKSE::log::info("Shared configuration loaded; enabled={}", shared.remember);
            }
            const bool ready = formsReady && inGame();
            bool restore = false;
            phial::Profile profile;
            {
                std::lock_guard lock(sharedMutex);
                if (epoch != shared.epoch || !shared.session) return;
                restore = ready && shared.restorePending;
                profile = shared.profile;
                if (restore) shared.restorePending = false;
            }
            bool restored = true;
            if (restore) {
                for (std::size_t i = 0; i < phial::count; ++i) {
                    if (!phial::equal(globals[i]->value, profile.values[i]) && !writeSetting(i, profile.values[i])) {
                        restored = false;
                        break;
                    }
                }
                SKSE::log::info("Shared settings applied after load; verified={}", restored);
            }
            if (ready) refreshHotkeyRegistration(epoch);
            const auto values = ready ? readValues() : phial::Values{};
            std::lock_guard lock(sharedMutex);
            if (epoch != shared.epoch || !shared.session) return;
            shared.pollPending = false;
            const bool wasReady = shared.ready;
            shared.ready = ready;
            if (!ready) {
                if (formsReady && !shared.configError) shared.status = "Waiting for loading to finish...";
                return;  // Keep restorePending for the loading-menu close event.
            }
            shared.values = values;
            if (restore && !restored) {
                shared.error = shared.configError = true;
                shared.status = "Shared settings could not be applied completely. Check the current values and WhitePhialMenu.log.";
            } else if (restore) {
                shared.status = "Your remembered settings were applied to this save.";
                shared.error = false;
            } else if (!wasReady && !shared.configError) {
                shared.status = "Changes apply to this character. Enable Remember settings across saves to share them.";
                shared.error = false;
            }
        });
    }

    bool writeSetting(std::size_t field, float value)
    {
        if (!formsReady || !phial::valid(field, value) || !globals[field]) return false;
        auto global = globals[field];
        if (global->IsDeleted() || (global->GetFormFlags() & RE::TESGlobal::RecordFlags::kConstant)) {
            SKSE::log::error("Refused deleted/constant setting {}", phial::editorIDs[field]);
            return false;
        }
        // Globals are stored in the save's Global Variables table, not a
        // TESForm change-flag record. No guessed AddChange mask is needed.
        // Avoid Script::CompileAndRun: v2 crashed in that engine path on 1170.
        // Both Apply and shared-profile restoration arrive on the game thread.
        const float previous = global->value;
        global->value = value;
        const bool verified = phial::equal(global->value, value);
        SKSE::log::info("Direct global update {}: {} -> {}; readback={}; verified={}",
            phial::editorIDs[field], previous, value, global->value, verified);
        return verified;
    }

    void requestApply(phial::Request request)
    {
        auto tasks = SKSE::GetTaskInterface();
        if (!tasks) return;
        {
            std::lock_guard lock(sharedMutex);
            if (!shared.ready || !shared.session || shared.applyPending || request.epoch != shared.epoch) return;
            shared.applyPending = true;
            shared.status = "Applying changes...";
            shared.error = false;
        }
        tasks->AddTask([request] {
            bool wasRemembering, retrySave;
            {
                std::lock_guard lock(sharedMutex);
                if (request.epoch != shared.epoch || !shared.session) return;
                wasRemembering = shared.remember;
                retrySave = shared.saveFailed || shared.configError;
            }
            if (!formsReady || !inGame()) {
                std::lock_guard lock(sharedMutex);
                shared.applyPending = false;
                shared.status = "Changes were not applied. Close the loading or main menu and try again.";
                shared.error = true;
                return;
            }
            auto current = readValues();
            if (!phial::canApply(request, request.epoch, current, wasRemembering, retrySave)) {
                std::lock_guard lock(sharedMutex);
                shared.applyPending = false;
                shared.values = current;
                shared.status = "A setting changed while you were editing, or a value is invalid. Use Discard edits, then try again.";
                shared.error = true;
                SKSE::log::warn("Rejected invalid or stale settings request");
                return;
            }
            bool success = true;
            for (std::size_t i = 0; i < phial::count; ++i) {
                if (!(request.dirty & (1u << i)) || phial::equal(current[i], request.desired[i])) continue;
                if (!writeSetting(i, request.desired[i])) { success = false; break; }
            }
            refreshHotkeyRegistration(request.epoch);
            const auto values = readValues();
            std::string saveError;
            if (success && (request.remember || wasRemembering || retrySave)) {
                saveError = phial::saveProfile(profilePath, { request.remember, values });
                if (saveError.empty()) SKSE::log::info("Shared settings saved; enabled={}", request.remember);
                else SKSE::log::error("Shared settings not saved: {}", saveError);
            }
            std::lock_guard lock(sharedMutex);
            if (request.epoch != shared.epoch || !shared.session) return;
            shared.values = values;
            shared.applyPending = false;
            shared.error = !success || !saveError.empty();
            shared.saveFailed = !saveError.empty();
            if (success && saveError.empty()) {
                shared.remember = request.remember;
                shared.profile = { request.remember, values };
                shared.configError = false;
                shared.status = request.remember ? "Applied and remembered for all saves and new games." :
                    "Applied. Automatic sharing is off; save your game to keep these values in this save.";
            } else if (!saveError.empty()) {
                shared.status = "Applied to this game, but could not remember the settings: " + saveError + " Press Apply to retry.";
            } else shared.status = "A change could not be verified. Current values are shown below; see WhitePhialMenu.log.";
        });
    }

    void __stdcall render()
    {
        static phial::Draft draft;
        static auto nextRefresh = Clock::time_point{};
        const auto now = Clock::now();
        if (now >= nextRefresh) {
            requestRefresh();
            nextRefresh = now + std::chrono::milliseconds(300);
        }
        Shared view;
        {
            std::lock_guard lock(sharedMutex);
            view = shared;
        }
        imgui::TextUnformatted("White Phial - Tweaks and Enhancements");
        imgui::Separator();
        if (!view.ready) {
            imgui::TextWrapped("%s", view.status.c_str());
            return;
        }
        draft.receive(view.epoch, view.values, view.remember);
        imgui::BeginDisabled(view.applyPending);

        bool enchanted = draft.request.desired[phial::repaired] >= 1;
        if (imgui::Checkbox("Fully re-enchanted", &enchanted)) draft.edit(phial::repaired, enchanted ? 1.0f : 0.0f);
        imgui::TextWrapped("Unlocks the phial's full potential after Quintus repairs it. This does not complete the original quest.");
        imgui::Spacing();

        float hours = draft.request.desired[phial::hours];
        imgui::SetNextItemWidth(300);
        if (imgui::InputFloat("Refill time (game hours)", &hours, 1.0f, 6.0f, "%.2f")) draft.edit(phial::hours, hours);
        if (imgui::Button("6 hours")) draft.edit(phial::hours, 6);
        imgui::SameLine();
        if (imgui::Button("12 hours")) draft.edit(phial::hours, 12);
        imgui::SameLine();
        if (imgui::Button("24 hours")) draft.edit(phial::hours, 24);
        imgui::SameLine();
        if (imgui::Button("48 hours")) draft.edit(phial::hours, 48);
        imgui::TextWrapped("In-game time, not real time. An active refill continues under the original mod's timer rules.");
        if ((draft.request.dirty & (1u << phial::hours)) && !phial::valid(phial::hours, hours))
            imgui::TextWrapped("Enter a refill time from 0.1 to 8760 hours.");
        imgui::Spacing();

        const auto keyLabel = phial::keyName(draft.request.desired[phial::hotkey]);
        imgui::SetNextItemWidth(300);
        if (imgui::BeginCombo("Use phial hotkey", keyLabel.c_str())) {
            for (const auto& key : phial::keys) {
                const bool selected = draft.request.desired[phial::hotkey] == key.code;
                if (imgui::Selectable(key.name, selected)) draft.edit(phial::hotkey, static_cast<float>(key.code));
                if (selected) imgui::SetItemDefaultFocus();
            }
            imgui::EndCombo();
        }
        if (imgui::Button("Default key: Numpad /")) draft.edit(phial::hotkey, 181);
        imgui::TextWrapped("The original mod's hotkey uses a filled potion phial. It does not use poisons. Choose a key that is free in your other mods.");
        imgui::Spacing();
        imgui::Separator();

        imgui::Checkbox("Remember settings across saves", &draft.request.remember);
        imgui::TextWrapped("Apply to remember all three settings, including re-enchantment status, for other saves and new characters.");
        imgui::Spacing();
        imgui::BeginDisabled(!draft.validEdits() && !view.saveFailed && !view.configError);
        if (imgui::Button("Apply changes")) requestApply(draft.request);
        imgui::EndDisabled();
        imgui::SameLine();
        if (imgui::Button("Discard edits")) {
            draft.reset(view.epoch, view.values, view.remember);
            requestRefresh();
        }
        imgui::EndDisabled();
        imgui::Spacing();
        if (view.error) imgui::TextWrapped("Error: %s", view.status.c_str());
        else imgui::TextWrapped("%s", view.status.c_str());
        if (!view.hotkeyStatus.empty()) imgui::TextWrapped("%s", view.hotkeyStatus.c_str());
        if (draft.request.dirty || draft.request.remember != draft.request.expectedRemember)
            imgui::TextUnformatted("You have unapplied changes.");
        const auto currentKey = phial::keyName(view.values[phial::hotkey]);
        imgui::TextWrapped("Current: %s | Refill: %.2f hours | Hotkey: %s",
            view.values[phial::repaired] >= 1 ? "Fully re-enchanted" : "Partially repaired",
            view.values[phial::hours], currentKey.c_str());
    }

    void registerMenu()
    {
        if (registered) return;
        const auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!module) {
            SKSE::log::error("SKSE Menu Framework is not loaded; settings panel unavailable");
            return;
        }
        // Check every UI export used here before a render callback can execute.
        // Old or mismatched framework builds cannot cause a null function call.
        constexpr const char* required[] = {
            "AddSectionItem", "igTextUnformatted", "igSeparator", "igTextWrappedV",
            "igBeginDisabled", "igEndDisabled", "igCheckbox", "igSpacing", "igSetNextItemWidth",
            "igInputFloat", "igButton", "igSameLine", "igBeginCombo", "igEndCombo",
            "igSelectable_Bool", "igSetItemDefaultFocus"
        };
        for (const auto name : required) {
            if (!GetProcAddress(module, name)) {
                SKSE::log::error("Menu Framework is missing export {}; install a compatible 3.x version", name);
                return;
            }
        }
        SKSEMenuFramework::SetSection("White Phial");
        SKSEMenuFramework::AddSectionItem("Settings", render);
        registered = true;
        SKSE::log::info("Registered menu: White Phial / Settings");
    }

    void resolveGlobals()
    {
        auto data = RE::TESDataHandler::GetSingleton();
        if (!data) return;
        // Local form ID and script name verified against the supplied original ESP/PEX.
        hotkeyEffect = data->LookupForm<RE::EffectSetting>(0xD4D, originalPlugin);
        SKSE::log::info("Original hotkey effect resolved={}", hotkeyEffect != nullptr);
        // Global EDIDs are retained by Skyrim itself. No EditorID extension or
        // load-order-dependent FormIDs are necessary. Reject ambiguous matches.
        std::array<unsigned, phial::count> matches{};
        for (auto global : data->GetFormArray<RE::TESGlobal>()) {
            if (!global || global->IsDeleted()) continue;
            const auto edid = global->GetFormEditorID();
            if (!edid) continue;
            for (std::size_t i = 0; i < phial::count; ++i) {
                if (_stricmp(edid, phial::editorIDs[i].data()) != 0) continue;
                ++matches[i];
                globals[i] = global;
            }
        }
        formsReady = true;
        for (std::size_t i = 0; i < phial::count; ++i) {
            auto origin = globals[i] ? globals[i]->GetFile(0) : nullptr;
            const bool valid = matches[i] == 1 && origin && _stricmp(origin->GetFilename().data(), originalPlugin) == 0;
            if (valid) SKSE::log::info("Resolved {} -> {:08X}; value={}", phial::editorIDs[i], globals[i]->GetFormID(), globals[i]->value);
            else {
                formsReady = false;
                SKSE::log::error("Missing/ambiguous global {} (matches={}) or unexpected owning plugin", phial::editorIDs[i], matches[i]);
            }
        }
    }

    class LoadMenus final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (!event) return RE::BSEventNotifyControl::kContinue;
            if (event->menuName == RE::MainMenu::MENU_NAME && event->opening) resetSession(false);
            if (!event->opening && (event->menuName == RE::LoadingMenu::MENU_NAME ||
                event->menuName == RE::MainMenu::MENU_NAME)) requestRefresh();
            return RE::BSEventNotifyControl::kContinue;
        }
    } loadMenus;

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        switch (message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            registerMenu();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            resolveGlobals();
            if (auto ui = RE::UI::GetSingleton()) ui->AddEventSink<RE::MenuOpenCloseEvent>(&loadMenus);
            registerMenu();
            break;
        case SKSE::MessagingInterface::kPreLoadGame:
            resetSession(false);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            resetSession(message->data != nullptr);  // SKSE passes the load-success bool in data.
            requestRefresh();
            break;
        case SKSE::MessagingInterface::kNewGame:
            resetSession(true);
            requestRefresh();
            break;
        default: break;
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({ 1, 1, 2, 0 });
    data.PluginName("WhitePhialMenu");
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
    *path /= "WhitePhialMenu.log";
    auto logger = std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("WhitePhialMenu 1.1.2; Skyrim 1.6.1170; direct global writes; live hotkey registration; shared settings supported");
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
