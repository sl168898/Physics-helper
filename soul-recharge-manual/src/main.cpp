#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <string>
#include "CoreBridge.h"
#include "InputRules.h"
#include "SKSEMenuFramework.h"

namespace {
using Json=nlohmann::json;
using Clock=std::chrono::steady_clock;
namespace Menu=SKSEMenuFramework;
namespace Gui=ImGuiMCP;
constexpr char preferencesPath[]="Data/SKSE/Plugins/SoulRechargeMenu.json";
constexpr std::array<const char*,10> boolKeys{"Enabled","StarFirst","UseAzuraStar","UseBlackStar","UseSoulGems","PreserveBlackGems","PreserveGrand","RangedWeapons","DualWield","Staves"};
std::atomic_uint32_t binding{87};
std::atomic_bool ready{}, capturing{}, apiBusy{};
std::atomic_uint64_t generation{};
std::mutex uiMutex,preferencesMutex;
Json edited, snapshot;
bool settingsValid{}, frameworkAvailable{}, previousMenuHotkey{true};
std::string status="Load a save to read the current recharge settings.";
std::string bridgeError;
Clock::time_point lastRequest{}, apiStarted{};

void setStatus(std::string text) {std::scoped_lock lock(uiMutex);status=std::move(text);}
const Json* field(const Json& object,const char* name) {
    if(!object.is_object()) return nullptr;
    if(auto it=object.find(name);it!=object.end()) return &*it;
    for(auto& child:object) if(child.is_object()) if(auto* found=field(child,name)) return found;
    return nullptr;
}
bool settingsFrom(const Json& value,Json& result) {
    result=Json{{"schema",1}};
    for(auto key:boolKeys) {
        const auto* entry=field(value,key);
        if(!entry || !entry->is_boolean()) return false;
        result[key]=*entry;
    }
    const auto* threshold=field(value,"ThresholdPercent");
    if(!threshold || !threshold->is_number_integer()) return false;
    const auto number=threshold->get<int>();
    if(number<0 || number>90) return false;
    result["ThresholdPercent"]=number;
    return true;
}
bool savePreferences(bool automatic,std::uint32_t key) {
    std::scoped_lock lock(preferencesMutex);
    const Json value{{"schema",1},{"AutomaticRecharge",automatic},{"RechargeKey",key}};
    const std::string path=preferencesPath, temporary=path+".tmp";
    const auto text=value.dump(2)+"\n";
    HANDLE file=CreateFileA(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    bool ok=file!=INVALID_HANDLE_VALUE;
    DWORD written{};
    if(ok) {ok=WriteFile(file,text.data(),static_cast<DWORD>(text.size()),&written,nullptr) && written==text.size() && FlushFileBuffers(file);CloseHandle(file);}
    if(ok) ok=MoveFileExA(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
    if(!ok) {DeleteFileA(temporary.c_str());setStatus("Could not save the mode or hotkey. Your previous selection is still active.");return false;}
    binding.store(key,std::memory_order_release);
    sr::bridge::automatic.store(automatic,std::memory_order_release);
    setStatus(automatic ? "Automatic recharge is active. Mode and hotkey saved." : "Manual recharge is active. Mode and hotkey saved.");
    return true;
}
void loadPreferences() {
    auto key=sr::bridge::rechargeKey();if(!sr::bindable(key)) key=87;
    binding=key;sr::bridge::automatic=false;
    std::error_code ec;
    if(!std::filesystem::exists(preferencesPath,ec)) return;
    try {
        std::ifstream file(preferencesPath);
        auto value=Json::parse(file);
        if(value.at("schema")!=1 || !value.at("AutomaticRecharge").is_boolean() || !value.at("RechargeKey").is_number_unsigned()) throw std::runtime_error("Invalid preferences");
        key=value.at("RechargeKey").get<std::uint32_t>();
        if(!sr::bindable(key)) throw std::runtime_error("Invalid key");
        binding=key;sr::bridge::automatic=value.at("AutomaticRecharge").get<bool>();
    } catch(const std::exception& e) {
        SKSE::log::warn("Invalid menu preferences: {}",e.what());
        setStatus("The saved mode/hotkey could not be read. Manual mode and the default hotkey are active; choose a mode to save new settings.");
    }
}
class Callback final:public RE::BSScript::IStackCallbackFunctor {
public:
    explicit Callback(std::function<void(RE::BSScript::Variable)> fn):fn_(std::move(fn)) {}
    void operator()(RE::BSScript::Variable value) override {fn_(value);}
    void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
private:
    std::function<void(RE::BSScript::Variable)> fn_;
};
void finishFailure(std::uint64_t token,const char* message) {
    if(generation.load()!=token) return;
    apiBusy=false;setStatus(message);
}
void readSnapshotOnGameThread(std::uint64_t token,std::string note) {
    if(generation.load()!=token) return;
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    if(!vm) {finishFailure(token,"The scripting engine is not ready. Load a save, then press Refresh.");return;}
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new Callback([token,note=std::move(note)](RE::BSScript::Variable value) {
        if(generation.load()!=token) return;
        try {
            if(!value.IsString()) throw std::runtime_error("No snapshot string");
            auto parsed=Json::parse(std::string(value.GetString()));
            Json settings;
            if(!settingsFrom(parsed,settings)) throw std::runtime_error("Missing settings");
            std::scoped_lock lock(uiMutex);
            snapshot=std::move(parsed);edited=std::move(settings);settingsValid=true;
            status=note.empty() ? "Current recharge settings loaded." : note;
        } catch(const std::exception& e) {
            SKSE::log::warn("Could not read original settings: {}",e.what());
            setStatus("Could not read the recharge settings. Load a save and press Refresh; existing settings have been preserved.");
        }
        apiBusy=false;
    }));
    if(!vm->DispatchStaticCall("AGH_SR_Native","GetSnapshotJson",RE::MakeFunctionArguments(),callback))
        finishFailure(token,"Could not call the original recharge mod. Check that its DLL and AGH_SR_Native.pex are installed.");
}
void requestSnapshot() {
    if(!ready || apiBusy.exchange(true)) return;
    const auto token=++generation;
    {std::scoped_lock lock(uiMutex);apiStarted=Clock::now();}
    SKSE::GetTaskInterface()->AddTask([token]{readSnapshotOnGameThread(token,{});});
}
void applySettings(Json settings) {
    if(!ready || apiBusy.exchange(true)) return;
    const auto token=++generation;
    {std::scoped_lock lock(uiMutex);apiStarted=Clock::now();status="Applying recharge settings...";}
    SKSE::GetTaskInterface()->AddTask([token,text=settings.dump()] {
        if(generation.load()!=token) return;
        auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
        if(!vm) {finishFailure(token,"The scripting engine is not ready.");return;}
        RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback(new Callback([token](RE::BSScript::Variable value) {
            if(generation.load()!=token) return;
            const bool applied=value.IsBool() && value.GetBool();
            SKSE::GetTaskInterface()->AddTask([token,applied] {
                readSnapshotOnGameThread(token,applied ? "Recharge settings applied and saved." : "The original mod rejected or could not save that change. Current settings are shown below.");
            });
        }));
        RE::BSFixedString json(text.c_str());
        if(!vm->DispatchStaticCall("AGH_SR_Native","ApplySettingsJson",RE::MakeFunctionArguments(std::move(json)),callback))
            finishFailure(token,"Could not apply the recharge settings. Existing settings have been preserved.");
    });
}
std::string keyName(std::uint32_t key) {
    char text[128]{};
    const LONG scan=static_cast<LONG>((key&0x7f)<<16)|((key&0x80)?1L<<24:0);
    if(GetKeyNameTextA(scan,text,sizeof(text))) return text;
    return "Key "+std::to_string(key);
}
void endCapture() {
    if(capturing.exchange(false)) Menu::SetHotkeyEnabled(previousMenuHotkey);
}
bool __stdcall captureInput(RE::InputEvent* event) {
    if(!capturing || !event || event->GetDevice()!=RE::INPUT_DEVICE::kKeyboard) return false;
    auto* button=event->AsButtonEvent();
    if(!button) return false;
    if(button->IsDown()) {
        const auto key=button->GetIDCode();
        if(key==1) {endCapture();setStatus("Hotkey change cancelled.");}
        else if(sr::bindable(key)) {
            savePreferences(sr::bridge::automatic.load(),key);endCapture();
        } else setStatus("Choose a keyboard key other than the console or a modifier. Escape cancels.");
    }
    return true;
}
bool gameplayBlocked() {
    auto* ui=RE::UI::GetSingleton();
    return !ui || ui->GameIsPaused() || ui->IsMenuOpen(RE::Console::MENU_NAME) || Menu::IsAnyBlockingWindowOpened();
}
using InputFunction=RE::BSEventNotifyControl(*)(void*,RE::InputEvent* const*,RE::BSTEventSource<RE::InputEvent*>*);
RE::BSEventNotifyControl inputRelay(void* self,RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>* source) {
    if(!events) return RE::BSEventNotifyControl::kContinue;
    const bool blocked=gameplayBlocked();
    for(auto* event=*events;event;event=event->next) {
        if(event->GetDevice()!=RE::INPUT_DEVICE::kKeyboard) continue;
        auto* button=event->AsButtonEvent();if(!button) continue;
        const auto action=sr::route(button->GetIDCode(),binding.load(),sr::bridge::snapshotKey(),button->IsDown(),ready.load(),blocked,capturing.load());
        if(action==sr::InputAction::ignore) continue;
        if(action==sr::InputAction::recharge) {
            const auto now=Clock::now();
            if(now-lastRequest<std::chrono::milliseconds(300)) continue;
            lastRequest=now;
        }
        // Private copy passed only to this mod's original sink, never broadcast.
        RE::ButtonEvent copy=*button;copy.next=nullptr;
        copy.idCode=action==sr::InputAction::recharge ? sr::bridge::rechargeKey() : sr::bridge::snapshotKey();
        RE::InputEvent* head=&copy;
        reinterpret_cast<InputFunction>(sr::bridge::originalInput)(self,&head,source);
    }
    return RE::BSEventNotifyControl::kContinue;
}
void checkbox(Json& settings,const char* key,const char* label) {
    bool selected=settings.at(key).get<bool>();
    if(Gui::Checkbox(label,&selected)) settings[key]=selected;
}
void __stdcall render() {
    Gui::TextUnformatted("Soul Gem Recharge");
    Gui::TextWrapped("Recharge equipped enchanted weapons with your filled soul gems. Choose when recharging happens.");
    if(!bridgeError.empty()) {Gui::Separator();Gui::TextWrapped("%s",bridgeError.c_str());return;}
    bool automatic=sr::bridge::automatic.load();
    if(Gui::Checkbox("Automatic recharge",&automatic)) savePreferences(automatic,binding.load());
    Gui::TextWrapped(automatic ? "Automatic mode: the original charge threshold and recharge rules are active. The hotkey also works." : "Manual mode: automatic requests are blocked. Press your recharge hotkey during gameplay to use the original manual recharge command.");
    const auto label=capturing.load() ? std::string("Press a key... (Escape cancels)") : "Recharge hotkey: "+keyName(binding.load());
    if(Gui::Button(label.c_str()) && !capturing) {
        previousMenuHotkey=Menu::IsHotkeyEnabled();Menu::SetHotkeyEnabled(false);capturing=true;
    }
    Gui::TextWrapped("Choose a key that is not used by another mod or by the menu itself. Mode and hotkey changes save immediately.");
    Gui::Separator();
    Json settings;std::string message;bool valid{},busy{};std::uint64_t editGeneration{};
    {
        std::scoped_lock lock(uiMutex);
        if(apiBusy && Clock::now()-apiStarted>std::chrono::seconds(10)) {
            ++generation;apiBusy=false;status="No reply from the recharge mod. Load a save and press Refresh.";
        }
        settings=edited;valid=settingsValid;message=status;busy=apiBusy.load();editGeneration=generation.load();
    }
    Gui::TextWrapped("%s",message.c_str());
    if(!busy && Gui::Button("Refresh current settings")) requestSnapshot();
    if(!valid) return;
    const auto before=settings;
    Gui::BeginDisabled(busy);
    Gui::TextUnformatted("Weapon and soul-gem settings");
    checkbox(settings,"Enabled","Enable weapon recharging");
    if(!settings["Enabled"].get<bool>()) Gui::TextWrapped("Recharging is disabled in both modes. Enable it and apply the settings below.");
    int threshold=settings["ThresholdPercent"].get<int>();
    if(Gui::SliderInt("Automatic charge threshold (%)",&threshold,0,90)) settings["ThresholdPercent"]=threshold;
    checkbox(settings,"RangedWeapons","Recharge bows and crossbows");
    checkbox(settings,"DualWield","Include the left-hand weapon");
    checkbox(settings,"Staves","Recharge staves");
    Gui::Separator();
    checkbox(settings,"StarFirst","Prefer reusable Stars");
    checkbox(settings,"UseAzuraStar","Use Azura's Star");
    checkbox(settings,"UseBlackStar","Use the Black Star");
    checkbox(settings,"UseSoulGems","Use ordinary soul gems");
    checkbox(settings,"PreserveBlackGems","Preserve black soul gems");
    checkbox(settings,"PreserveGrand","Preserve grand souls");
    Gui::TextWrapped("The original mod's soul selection, reusable-gem rules, and charge checks apply in both modes.");
    if(settings!=before && !busy) {std::scoped_lock lock(uiMutex);if(editGeneration==generation.load() && !apiBusy) edited=settings;}
    if(Gui::Button("Apply weapon and soul-gem settings")) applySettings(settings);
    Gui::EndDisabled();
    Gui::Separator();
    std::string result;
    {std::scoped_lock lock(uiMutex);for(auto name:{"reason","settingsReason"}) if(auto* value=field(snapshot,name)) result+=std::string(name==std::string("reason")?"Last result: ":"   Settings: ")+(value->is_string()?value->get<std::string>():value->dump());}
    if(!result.empty()) Gui::TextWrapped("%s",result.c_str());
}
void __stdcall menuEvent(Menu::Model::EventType type) {
    if(type==Menu::Model::kCloseMenu) endCapture();
    if(type==Menu::Model::kOpenMenu && ready) requestSnapshot();
}
void message(SKSE::MessagingInterface::Message* message) {
    switch(message->type) {
    case SKSE::MessagingInterface::kPostPostLoad: {
        frameworkAvailable=GetModuleHandleW(L"SKSEMenuFramework.dll") && Menu::GetMenuFrameworkVersion()>=3.18f;
        if(sr::bridge::install(GetModuleHandleW(L"AGH_SoulRecharge_G0.dll"),bridgeError)) {
            loadPreferences();
            if(!sr::bridge::installInput(reinterpret_cast<std::uintptr_t>(&inputRelay))) {
                bridgeError="Could not install the hotkey controls.";sr::bridge::uninstallForTest();
            }
        }
        if(frameworkAvailable) {
            Menu::SetSection("Soul Gem Recharge");Menu::AddSectionItem("Settings",render);
            Menu::AddInputEvent(captureInput);Menu::AddEvent(menuEvent,0.0f);
        } else SKSE::log::error("SKSE Menu Framework 3.18 or newer is required for the settings page. The saved mode and hotkey remain active.");
        SKSE::log::info("Bridge installed: {}; automatic recharge: {}; hotkey: {}",sr::bridge::gateInstalled,sr::bridge::automatic.load(),binding.load());
        break;
    }
    case SKSE::MessagingInterface::kPreLoadGame:
        ready=false;endCapture();++generation;apiBusy=false;
        {std::scoped_lock lock(uiMutex);settingsValid=false;}
        break;
    case SKSE::MessagingInterface::kNewGame:
    case SKSE::MessagingInterface::kPostLoadGame:
        if(message->type==SKSE::MessagingInterface::kPostLoadGame && message->dataLen && !message->data) break;
        ready=sr::bridge::gateInstalled && sr::bridge::inputInstalled;
        if(ready) requestSnapshot();
        else RE::DebugNotification("Soul Gem Recharge menu could not start. Check SoulRechargeMenu.log.");
        if(!frameworkAvailable) RE::DebugNotification("Soul Gem Recharge: install SKSE Menu Framework 3.18 or newer for settings.");
        break;
    default:break;
    }
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
    SKSE::PluginVersionData data{};data.PluginVersion({1,2,0,0});data.PluginName("SoulRechargeMenu");
    data.AuthorName("Physics-helper contributors");data.UsesAddressLibrary(true);data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}});return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if(skse->RuntimeVersion()!=REL::Version{1,6,1170,0}) return false;
    SKSE::Init(skse);
    if(auto path=SKSE::log::log_directory()) {
        *path/="SoulRechargeMenu.log";
        auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);
        auto logger=std::make_shared<spdlog::logger>("SoulRechargeMenu",std::move(sink));
        spdlog::set_default_logger(std::move(logger));spdlog::flush_on(spdlog::level::info);
    }
    SKSE::log::info("Soul Recharge Menu 1.2.0; Skyrim 1.6.1170; original core 1.1.0");
    return SKSE::GetMessagingInterface()->RegisterListener(message);
}
