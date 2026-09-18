#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <vector>
#include "SKSEMenuFramework.h"
#include "Config.h"

namespace
{
    namespace imgui = ImGuiMCP;
    using Clock = std::chrono::steady_clock;
    constexpr auto originalPlugin = "The White Phial - Tweaks and Enhancements.esp";
    const std::filesystem::path configPath = "Data/SKSE/Plugins/WhitePhialWidget.ini";
    std::mutex stateMutex;
    widget::Session session;
    // Forms are resolved at DataLoaded. Inventory and UI reads occur only in
    // SKSE game-thread tasks; the render callbacks only see copied snapshots.
    std::vector<RE::TESBoundObject*> potions;
    RE::TESBoundObject* emptyPhial = nullptr;
    RE::TESBoundObject* poisonPhial = nullptr;
    bool formsReady = false;
    bool registered = false;
    SKSEMenuFramework::Model::HudElement* hud = nullptr; // Process lifetime.
    widget::Settings settings, savedSettings; // Render thread after plugin load.
    std::string settingsMessage;
    Clock::time_point nextPoll{}, previewUntil{}, retryTextures{};
    widget::State previewState = widget::State::potion;
    std::array<imgui::ImTextureID, 3> textures{};
    bool textureErrorLogged = false;

    void resetSession(bool active)
    {
        std::lock_guard lock(stateMutex);
        session.reset(active);
    }

    widget::Snapshot snapshot()
    {
        std::lock_guard lock(stateMutex);
        return session.view;
    }

    void requestRefresh()
    {
        // Called by render callbacks, even when the indicator is hidden.
        auto now = Clock::now();
        if (now < nextPoll) return;
        nextPoll = now + std::chrono::milliseconds(200);
        std::uint64_t ticket;
        {
            std::lock_guard lock(stateMutex);
            ticket = session.request();
        }
        if (!ticket) return;
        SKSE::GetTaskInterface()->AddTask([ticket] {
            {
                std::lock_guard lock(stateMutex);
                if (ticket != session.epoch || !session.pending || !session.view.active) return;
            }
            // Never hold our mutex across game/UI calls: menu notifications
            // also acquire it and may arrive while the UI owns an internal lock.
            auto publish = [ticket](const widget::Snapshot& value) {
                std::lock_guard lock(stateMutex);
                if (ticket != session.epoch || !session.pending) return;
                if (value.ready && !value.blocked && value.state != session.view.state)
                    SKSE::log::info("White Phial inventory state: {}", widget::label(value.state));
                session.finish(ticket, value);
            };
            widget::Snapshot out;
            out.active = true;
            out.ready = formsReady;
            auto ui = RE::UI::GetSingleton();
            auto player = RE::PlayerCharacter::GetSingleton();
            if (!ui || !player) { publish(out); return; }
            out.blocked = !ui->IsShowingMenus() || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
                ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) || !ui->IsMenuOpen(RE::HUDMenu::MENU_NAME);
            out.inMenu = ui->GameIsPaused();
            // Include menus that some load orders configure not to pause.
            constexpr std::string_view menus[] = { "InventoryMenu", "MagicMenu", "FavoritesMenu",
                "ContainerMenu", "BarterMenu", "Crafting Menu", "Dialogue Menu", "Console", "MapMenu",
                "Journal Menu", "TweenMenu", "Book Menu", "Lockpicking Menu", "Sleep/Wait Menu", "RaceSex Menu" };
            for (auto menu : menus) out.inMenu = out.inMenu || ui->IsMenuOpen(menu);
            if (out.ready && !out.blocked) {
                const auto counts = player->GetInventoryCounts([](RE::TESBoundObject& object) {
                    return &object == emptyPhial || &object == poisonPhial ||
                        std::find(potions.begin(), potions.end(), &object) != potions.end();
                }, true); // Read-only: do not force container initialization.
                auto has = [&](RE::TESBoundObject* object) {
                    auto it = counts.find(object);
                    return it != counts.end() && it->second > 0;
                };
                const bool potion = std::any_of(potions.begin(), potions.end(), has);
                out.state = widget::classify(has(emptyPhial), potion, has(poisonPhial));
            }
            publish(out);
        });
    }

    void loadTextures()
    {
        if (Clock::now() < retryTextures) return;
        retryTextures = Clock::now() + std::chrono::seconds(5);
        constexpr const char* names[] = { "empty", "filled", "poison" };
        bool missing = false;
        for (std::size_t i = 0; i < textures.size(); ++i) {
            if (!textures[i]) textures[i] = SKSEMenuFramework::LoadTexture(
                std::string("Data/SKSE/Plugins/WhitePhialWidget/") + names[i] + ".png");
            missing |= !textures[i];
        }
        if (missing && !textureErrorLogged) {
            SKSE::log::warn("Widget texture not loaded yet; check SKSE/Plugins/WhitePhialWidget/*.png if it persists");
            textureErrorLogged = true;
        }
    }

    imgui::ImU32 color(unsigned r, unsigned g, unsigned b, float opacity)
    {
        return r | (g << 8) | (b << 16) | (static_cast<unsigned>(255 * opacity / 100) << 24);
    }

    void __stdcall renderHUD()
    {
        requestRefresh();
        auto view = snapshot();
        const bool preview = Clock::now() < previewUntil;
        view.inMenu |= SKSEMenuFramework::IsAnyBlockingWindowOpened();
        if (!widget::visible(settings, view, preview)) return;
        const auto state = preview ? previewState : view.state;
        const auto io = imgui::GetIO();
        const auto font = imgui::GetFont();
        if (!io || !font || io->DisplaySize.x <= 0 || io->DisplaySize.y <= 0) return;
        loadTextures();
        auto draw = imgui::GetForegroundDrawList();
        if (!draw) return;
        auto cfg = settings;
        const auto texture = textures[state == widget::State::potion ? 1 : state == widget::State::poison ? 2 : 0];
        if (!texture) cfg.showLabel = true; // A readable status remains if an asset is missing.
        auto provisional = widget::layout(cfg, io->DisplaySize.x, io->DisplaySize.y, 0);
        auto measure = [&](const char* text, float size) {
            return imgui::ImFontManger::CalcTextSizeA(font, size * provisional.unit, 100000.0f, 0.0f, text, nullptr, nullptr).x;
        };
        const auto position = widget::layout(cfg, io->DisplaySize.x, io->DisplaySize.y,
            std::max(measure("WHITE PHIAL", 14), measure(widget::label(state), 19)));
        const auto x = position.x, y = position.y, u = position.unit;
        auto tint = color(255, 255, 255, cfg.opacity);
        if (texture) imgui::ImDrawListManager::AddImage(draw, texture, {x, y}, {x + 56*u, y + 56*u}, {0, 0}, {1, 1}, tint);
        if (!cfg.showLabel) return;
        auto text = [&](const char* value, float size, float offset, imgui::ImU32 shade) {
            const imgui::ImVec2 pos{x + 62*u, y + offset*u};
            imgui::ImDrawListManager::AddText(draw, font, size*u, {pos.x+u, pos.y+u}, color(0, 0, 0, cfg.opacity), value);
            imgui::ImDrawListManager::AddText(draw, font, size*u, pos, shade, value);
        };
        text("WHITE PHIAL", 14, 8, color(199, 190, 169, cfg.opacity));
        auto shade = state == widget::State::potion ? color(246, 217, 148, cfg.opacity) :
            state == widget::State::poison ? color(166, 219, 162, cfg.opacity) : color(202, 207, 214, cfg.opacity);
        text(widget::label(state), 19, 26, shade);
    }

    void __stdcall renderSettings()
    {
        requestRefresh();
        const auto view = snapshot();
        imgui::TextUnformatted("White Phial Status Widget - SKSE 2.0");
        imgui::TextWrapped("Shows the White Phial you are carrying. Grey: empty. Gold: filled potion. Green: filled poison.");
        if (!view.active) imgui::TextUnformatted("Load a game to read the phial in your inventory.");
        else if (!view.ready) imgui::TextWrapped("Phial forms unavailable. Enable The White Phial - Tweaks and Enhancements.esp and check WhitePhialWidget.log.");
        else imgui::TextWrapped("Current inventory: %s", widget::label(view.state));
        imgui::Separator();
        imgui::Checkbox("Enable widget", &settings.enabled);
        imgui::Checkbox("Show text label", &settings.showLabel);
        imgui::Checkbox("Hide when the phial is not carried", &settings.hideAbsent);
        imgui::Checkbox("Hide in menus", &settings.hideInMenus);
        imgui::SliderFloat("Horizontal position", &settings.x, 0, 100, "%.1f%%", imgui::ImGuiSliderFlags_AlwaysClamp);
        imgui::SliderFloat("Vertical position", &settings.y, 0, 100, "%.1f%%", imgui::ImGuiSliderFlags_AlwaysClamp);
        imgui::SliderFloat("Size", &settings.scale, 50, 250, "%.0f%%", imgui::ImGuiSliderFlags_AlwaysClamp);
        imgui::SliderFloat("Opacity", &settings.opacity, 10, 100, "%.0f%%", imgui::ImGuiSliderFlags_AlwaysClamp);
        if (!widget::valid(settings)) {
            settings = savedSettings;
            settingsMessage = "Invalid display value; saved settings restored.";
        }
        imgui::TextWrapped("Changes take effect immediately. Save settings to keep them across saves and game restarts.");
        if (imgui::Button("Preview empty")) { previewState = widget::State::empty; previewUntil = Clock::now() + std::chrono::seconds(10); }
        imgui::SameLine();
        if (imgui::Button("Preview potion")) { previewState = widget::State::potion; previewUntil = Clock::now() + std::chrono::seconds(10); }
        imgui::SameLine();
        if (imgui::Button("Preview poison")) { previewState = widget::State::poison; previewUntil = Clock::now() + std::chrono::seconds(10); }
        imgui::TextWrapped("Preview lasts 10 seconds and changes only the display.");
        if (imgui::Button("Save settings")) {
            const auto error = widget::saveConfig(configPath, settings);
            if (error.empty()) { savedSettings = settings; settingsMessage = "Widget settings saved."; }
            else { settingsMessage = error; SKSE::log::error("{}", error); }
        }
        imgui::SameLine();
        if (imgui::Button("Restore saved")) { settings = savedSettings; settingsMessage = "Saved widget settings restored."; }
        imgui::SameLine();
        if (imgui::Button("Reset defaults")) { settings = {}; settingsMessage = "Defaults restored. Save settings to keep them."; }
        if (!(settings == savedSettings)) imgui::TextUnformatted("Unsaved widget settings.");
        if (!settingsMessage.empty()) imgui::TextWrapped("%s", settingsMessage.c_str());
    }

    void registerUI()
    {
        if (registered) return;
        const auto module = GetModuleHandleW(L"SKSEMenuFramework.dll");
        if (!module) { SKSE::log::error("SKSE Menu Framework 3.x is not loaded; widget unavailable"); return; }
        constexpr const char* required[] = { "RegisterHudElement", "AddSectionItem", "LoadTexture",
            "IsAnyBlockingWindowOpened", "igGetIO", "igGetFont", "igGetForegroundDrawList_Nil",
            "ImDrawList_AddImage", "ImDrawList_AddText_FontPtr", "ImFont_CalcTextSizeA",
            "igTextUnformatted", "igTextWrappedV", "igSeparator", "igCheckbox", "igSliderFloat", "igButton", "igSameLine" };
        for (auto name : required) {
            if (!GetProcAddress(module, name)) {
                SKSE::log::error("Missing framework export {}; use a compatible SKSE Menu Framework 3.x build", name);
                return;
            }
        }
        SKSEMenuFramework::SetSection("White Phial");
        SKSEMenuFramework::AddSectionItem("Widget", renderSettings);
        hud = SKSEMenuFramework::AddHudElement(renderHUD);
        registered = true;
        SKSE::log::info("Registered native HUD and White Phial / Widget settings");
    }

    void resolveForms()
    {
        std::lock_guard lock(stateMutex);
        auto data = RE::TESDataHandler::GetSingleton();
        if (!data) return;
        emptyPhial = data->LookupForm<RE::TESObjectMISC>(0x2C25A, "Skyrim.esm");
        poisonPhial = data->LookupForm<RE::AlchemyItem>(0x80C, originalPlugin);
        auto list = data->LookupForm<RE::BGSListForm>(0xD4B, originalPlugin);
        potions.clear();
        if (list) list->ForEachForm([](RE::TESForm* form) {
            if (auto potion = form ? form->As<RE::AlchemyItem>() : nullptr) potions.push_back(potion);
            return RE::BSContainer::ForEachResult::kContinue;
        });
        formsReady = emptyPhial && poisonPhial && !potions.empty();
        SKSE::log::info("Resolved phial forms: empty={}, poison={}, filled potions={}, ready={}",
            emptyPhial != nullptr, poisonPhial != nullptr, potions.size(), formsReady);
        if (!formsReady) SKSE::log::error("Expected forms are missing from {}", originalPlugin);
    }

    class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
    {
        RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
        {
            if (event && event->opening) {
                if (event->menuName == RE::MainMenu::MENU_NAME) resetSession(false);
                else if (event->menuName == RE::LoadingMenu::MENU_NAME) {
                    std::lock_guard lock(stateMutex);
                    session.view.blocked = true;
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    } menuEvents;

    void onMessage(SKSE::MessagingInterface::Message* message)
    {
        if (!message) return;
        switch (message->type) {
        case SKSE::MessagingInterface::kPostLoad: registerUI(); break;
        case SKSE::MessagingInterface::kDataLoaded:
            resolveForms();
            if (auto ui = RE::UI::GetSingleton()) ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuEvents);
            registerUI();
            break;
        case SKSE::MessagingInterface::kPreLoadGame: resetSession(false); break;
        case SKSE::MessagingInterface::kPostLoadGame: resetSession(message->data != nullptr); break;
        case SKSE::MessagingInterface::kNewGame: resetSession(true); break;
        default: break;
        }
    }
}

extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version = [] {
    SKSE::PluginVersionData data{};
    data.PluginVersion({2, 0, 0, 0});
    data.PluginName("WhitePhialWidget");
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
    *path /= "WhitePhialWidget.log";
    auto logger = std::make_shared<spdlog::logger>("global",
        std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true));
    spdlog::set_default_logger(std::move(logger));
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
    SKSE::Init(skse);
    const auto config = widget::loadConfig(configPath);
    settings = savedSettings = config.settings;
    if (!config.error.empty()) { settingsMessage = config.error; SKSE::log::warn("{} Using defaults.", config.error); }
    SKSE::log::info("WhitePhialWidget 2.0.0; Skyrim 1.6.1170; native read-only inventory HUD; 200 ms polling");
    return SKSE::GetMessagingInterface()->RegisterListener(onMessage);
}
