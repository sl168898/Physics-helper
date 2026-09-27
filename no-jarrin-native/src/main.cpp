#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "BlockSlots.h"

namespace
{
    constexpr auto modFile = "more plants all.esp";
    std::atomic_bool ready{false};
    std::atomic_bool session{false};
    std::atomic_uint64_t generation{0};

    struct Forms
    {
        RE::BGSListForm* seeds{};
        RE::BGSListForm* crops{};
        RE::IngredientItem* root{};
        RE::TESQuest* marker{};
        explicit operator bool() const { return seeds && crops && root && marker; }
    };

    Forms resolve()
    {
        const auto data = RE::TESDataHandler::GetSingleton();
        if (!data) return {};
        return {
            data->LookupForm<RE::BGSListForm>(0x8247, "HearthFires.esm"),
            data->LookupForm<RE::BGSListForm>(0x8246, "HearthFires.esm"),
            data->LookupForm<RE::IngredientItem>(0x1BCBC, "Skyrim.esm"),
            data->LookupForm<RE::TESQuest>(0xD66, modFile)
        };
    }

    std::size_t size(const RE::BGSListForm* list)
    {
        return list->forms.size() +
            (list->scriptAddedTempForms ? list->scriptAddedTempForms->size() : 0);
    }

    void restrictPlanting(const char* reason, bool report)
    {
        // Resolve on the game thread. No game-form pointers survive a queued task
        // or a save change. We do not resize arrays, adjust counts, or call Revert.
        const auto forms = resolve();
        if (!forms) {
            SKSE::log::error("Missing required forms. Enable the supplied more plants all.esp and HearthFires.esm.");
            return;
        }
        const auto seedCount = size(forms.seeds);
        const auto cropCount = size(forms.crops);
        const auto addedCount = forms.seeds->scriptAddedFormCount;
        auto changed = no_jarrin::blockSlots(forms.seeds->forms,
            static_cast<RE::TESForm*>(forms.root), static_cast<RE::TESForm*>(forms.marker));
        if (forms.seeds->scriptAddedTempForms) {
            changed += no_jarrin::blockSlots(*forms.seeds->scriptAddedTempForms,
                forms.root->GetFormID(), forms.marker->GetFormID());
        }
        const bool blocked = !forms.seeds->HasForm(forms.root);
        if (report || changed || !blocked) {
            SKSE::log::info("{}: JarrinRoot={:08X}; seedList={:08X}; marker={:08X}; replaced={}; seeds={}; crops={}; added={}; root_present={}",
                reason, forms.root->GetFormID(), forms.seeds->GetFormID(), forms.marker->GetFormID(),
                changed, seedCount, cropCount, addedCount, !blocked);
            if (blocked) {
                SKSE::log::info("VERIFIED: Jarrin Root is absent from the Hearthfire planting seed list. Other seed slots and the crop list are unchanged.");
            } else {
                SKSE::log::error("Restriction failed: Jarrin Root remains in the planting seed list.");
            }
            if (seedCount != cropCount) {
                SKSE::log::warn("The seed/crop list sizes differ. This pre-existing mismatch was left unchanged; Jarrin Root blocking does not require equal sizes.");
            }
        }
    }

    void schedule(const char* reason, bool report = false)
    {
        if (!ready.load() || !session.load()) return;
        const auto epoch = generation.load();
        if (const auto tasks = SKSE::GetTaskInterface()) {
            tasks->AddTask([epoch, reason, report] {
                if (ready.load() && session.load() && generation.load() == epoch)
                    restrictPlanting(reason, report);
            });
        }
    }

    class Events final :
        public RE::BSTEventSink<SKSE::ModCallbackEvent>,
        public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
        public RE::BSTEventSink<RE::TESActivateEvent>
    {
    public:
        using Result = RE::BSEventNotifyControl;
        static Events& get() { static Events events; return events; }

        Result ProcessEvent(const SKSE::ModCallbackEvent* event,
            RE::BSTEventSource<SKSE::ModCallbackEvent>*) override
        {
            if (event && event->eventName == "FLM_SetupDone") schedule("FLM_SetupDone", true);
            return Result::kContinue;
        }

        Result ProcessEvent(const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (event && event->opening && event->menuName == RE::ContainerMenu::MENU_NAME)
                schedule("ContainerMenu opened");
            return Result::kContinue;
        }

        Result ProcessEvent(const RE::TESActivateEvent* event,
            RE::BSTEventSource<RE::TESActivateEvent>*) override
        {
            if (event && event->actionRef.get() == RE::PlayerCharacter::GetSingleton())
                schedule("Player activation");
            return Result::kContinue;
        }
    };

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        switch (message->type) {
        case SKSE::MessagingInterface::kPostLoad:
            if (const auto source = SKSE::GetModCallbackEventSource())
                source->AddEventSink(&Events::get());
            break;
        case SKSE::MessagingInterface::kDataLoaded: {
            const auto forms = resolve();
            ready.store(static_cast<bool>(forms));
            if (!ready.load()) {
                SKSE::log::error("Required forms missing: seeds={} crops={} Jarrin={} marker={}. Install the complete v5 update for more plants all.esp.",
                    forms.seeds != nullptr, forms.crops != nullptr, forms.root != nullptr, forms.marker != nullptr);
                break;
            }
            if (const auto ui = RE::UI::GetSingleton())
                ui->AddEventSink<RE::MenuOpenCloseEvent>(&Events::get());
            if (const auto source = RE::ScriptEventSourceHolder::GetSingleton())
                source->AddEventSink<RE::TESActivateEvent>(&Events::get());
            SKSE::log::info("Ready. Waiting for a loaded save or new game. Blocking uses native form-list slots; Papyrus removal is not required.");
            break;
        }
        case SKSE::MessagingInterface::kPreLoadGame:
            session.store(false);
            generation.fetch_add(1);
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            session.store(message->data != nullptr);
            if (session.load()) schedule("PostLoadGame", true);
            break;
        case SKSE::MessagingInterface::kNewGame:
            generation.fetch_add(1);
            session.store(true);
            schedule("NewGame", true);
            break;
        default:
            break;
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({5, 0, 0, 0});
    data.PluginName("NoJarrinPlanting");
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
    *path /= "NoJarrinPlanting.log";
    spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true)));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    SKSE::log::info("NoJarrinPlanting 5.0.0 beta; Skyrim 1.6.1170. Native Hearthfire seed restriction; More Plants base variant.");
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
