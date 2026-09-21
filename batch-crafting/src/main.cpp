#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "SKSEMenuFramework.h"
#include "Batch.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>

namespace {
using Sub=RE::CraftingSubMenus::ConstructibleObjectMenu;
using Clock=std::chrono::steady_clock;
using Advance=void (*)(RE::CraftingMenu*,float,std::uint32_t);
REL::Relocation<Advance> originalAdvance;
// Confirmed ConstructibleObjectMenu crafting routine. AE address-library ID
// documented by Yes Im Sure NG (51369); no prompt patch or copied recipe logic.
REL::Relocation<void (*)(Sub*)> craft;
SKSEMenuFramework::Model::WindowInterface* window=nullptr;
std::atomic<std::uint64_t> epoch{0};
std::atomic<bool> cancel{false};
std::uint32_t hotkey=66; // DirectInput F8; configurable at launch.
bool menuReady=false;
std::recursive_mutex controller; // Events may re-enter during a native craft.
batch::Job job;
Clock::time_point nextStep{};
struct Snapshot {
 batch::Selection selection{};
 std::string name,status="Highlight a cooking or forge recipe, then press F8.";
 std::uint32_t max{},done{},target{};
 bool running=false;
};
std::mutex gate;
Snapshot shared;

bool unobstructed() {
 auto* ui=RE::UI::GetSingleton();
 return ui && ui->IsMenuOpen(RE::CraftingMenu::MENU_NAME) &&
  !ui->IsMenuOpen(RE::MessageBoxMenu::MENU_NAME) && !ui->IsMenuOpen(RE::Console::MENU_NAME) &&
  !ui->IsMenuOpen(RE::JournalMenu::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
}
Sub* submenu(RE::CraftingMenu* menu) {
 return menu ? skyrim_cast<Sub*>(menu->GetCraftingSubMenu()) : nullptr;
}
RE::BGSConstructibleObject* recipe(Sub* sub) {
 return sub && sub->currentCobjIdx<sub->crafts.size() ? sub->crafts[sub->currentCobjIdx].constructibleObject : nullptr;
}
batch::Selection selection(RE::CraftingMenu* menu,Sub* sub,RE::BGSConstructibleObject* r) {
 return {epoch.load(),reinterpret_cast<std::uintptr_t>(menu),reinterpret_cast<std::uintptr_t>(sub),
 r ? r->GetFormID():0,r && r->createdItem ? r->createdItem->GetFormID():0,r ? r->data.numConstructed:0u};
}
std::int64_t count(RE::TESBoundObject* object) {
 auto* player=RE::PlayerCharacter::GetSingleton();if(!player || !object)return -1;
 auto inventory=player->GetInventoryCounts([object](RE::TESBoundObject& o){return &o==object;});
 auto it=inventory.find(object);return it==inventory.end()?0:it->second;
}
std::uint32_t available(Sub* sub,RE::BGSConstructibleObject* r,std::string& reason) {
 auto* player=RE::PlayerCharacter::GetSingleton();
 auto* output=r && r->createdItem ? r->createdItem->As<RE::TESBoundObject>():nullptr;
 if(!player || !r || !output || !r->data.numConstructed) {reason="No valid crafting recipe selected.";return 0;}
 // Use the game-populated enabled flag as well as native conditions/costs.
 // No assumption that the list position equals the engine's recipe index.
 RE::GFxValue entry,enabled,id;
 if(!sub->itemList.GetMember("selectedEntry",&entry) || !entry.IsObject() ||
    !entry.GetMember("enabled",&enabled) || !enabled.IsBool() || !enabled.GetBool()) {
  reason="The selected recipe is unavailable (or this crafting interface is unsupported).";return 0;
 }
 if(entry.GetMember("formId",&id) && id.IsNumber() && id.GetUInt()!=output->GetFormID()) {
  reason="The highlighted item and native recipe do not match. Select the recipe again.";return 0;
 }
 if(!r->conditions.IsTrue(player,player)) {reason="The recipe requirements are not met.";return 0;}
 auto inventory=player->GetInventoryCounts();
 std::vector<batch::Material> materials;
 bool valid=r->requiredItems.containerObjects && r->requiredItems.numContainerObjects>0 && r->requiredItems.numContainerObjects<=256;
 if(valid) for(std::uint32_t i=0;i<r->requiredItems.numContainerObjects;++i) {
  auto* m=r->requiredItems.containerObjects[i];
  if(!m || !m->obj) {valid=false;break;}
  auto it=inventory.find(m->obj);
  materials.push_back({m->obj->GetFormID(),m->count,it==inventory.end()?0:it->second});
 }
 auto n=valid ? batch::maximum(materials,output->GetFormID()):0;
 if(!n)reason="Insufficient materials, or a free/self-producing recipe that cannot be safely batched.";
 return n;
}
void status(std::string text) {
 std::lock_guard lock(gate);shared.status=std::move(text);shared.running=job.active;
 shared.done=job.completed;shared.target=job.requested;
}
void stop(const std::string& why) {
 job.stop();status(why);
 SKSE::log::info("Batch stopped: {}; confirmed crafts={}/{}",why,job.completed,job.requested);
 RE::DebugNotification(why.c_str());
}
void requestOpen() {
 const auto ticket=epoch.load();
 SKSE::GetTaskInterface()->AddUITask([ticket] {
  std::lock_guard controlLock(controller);
  if(ticket!=epoch.load() || !unobstructed())return;
  if(job.active){cancel=true;return;}
  auto menu=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();auto* sub=submenu(menu.get());
  if(!sub) {RE::DebugNotification("Batch Crafting supports cooking and making items, not alchemy, enchanting or tempering.");return;}
  auto* r=recipe(sub);Snapshot s;s.selection=selection(menu.get(),sub,r);
  s.name=r&&r->createdItem&&r->createdItem->GetName()?r->createdItem->GetName():"No recipe";
  s.max=available(sub,r,s.status);
  if(s.max)s.status="Choose how many times to craft this recipe.";
  {std::lock_guard lock(gate);shared=std::move(s);}
  if(window)window->IsOpen=true;
 });
}
void requestStart(const Snapshot& s,std::uint32_t n) {
 SKSE::GetTaskInterface()->AddUITask([s,n] {
  std::lock_guard controlLock(controller);
  if(s.selection.session!=epoch.load() || !unobstructed())return;
  auto menu=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();auto* sub=submenu(menu.get());auto* r=recipe(sub);
  std::string reason;
  if(selection(menu.get(),sub,r)!=s.selection){status("Selection changed. Refresh the selected recipe.");return;}
  auto max=available(sub,r,reason);
  if(!max || !job.start(s.selection,n,max)){status(reason.empty()?"The quantity is unavailable, or a batch is already running.":reason);return;}
  cancel=false;nextStep=Clock::now()+std::chrono::milliseconds(250);
  if(window)window->IsOpen=false;
  if(auto* main=SKSEMenuFramework::GetMainWindow())main->IsOpen=false;
  status("Batch started. Press the batch hotkey again or change selection to stop.");
  SKSE::log::info("Batch started: recipe={:08X}, crafts={}, units/craft={}",s.selection.recipe,n,s.selection.units);
 });
}
void tick(RE::CraftingMenu* menu) {
 std::lock_guard controlLock(controller);
 if(!job.active)return;
 if(cancel.exchange(false)){stop("Batch cancelled.");return;}
 auto live=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();
 if(!unobstructed() || live.get()!=menu){stop("Batch stopped: crafting menu closed.");return;}
 auto* sub=submenu(menu);auto* r=recipe(sub);
 if(!unobstructed() || selection(menu,sub,r)!=job.selection){stop("Batch stopped: menu or recipe changed.");return;}
 if(window && window->IsOpen.load())return;
 if(auto* main=SKSEMenuFramework::GetMainWindow();main && main->IsOpen.load())return;
 if(Clock::now()<nextStep)return;
 auto* output=r->createdItem->As<RE::TESBoundObject>();
 if(job.awaiting) {
  if(!job.confirm(count(output))){stop("Batch stopped: the previous craft's output could not be confirmed.");return;}
  if(!job.active){const auto text="Batch complete: "+std::to_string(job.completed)+" crafts.";status(text);RE::DebugNotification(text.c_str());return;}
 }
 std::string reason;auto max=available(sub,r,reason);
 if(!job.canCraft(selection(menu,sub,r),max)){stop(reason.empty()?"Batch stopped: recipe unavailable.":reason);return;}
 const auto before=count(output);if(before<0){stop("Batch stopped: inventory unavailable.");return;}
 job.submitted(before);
 craft(sub); // Engine handles consumption, output, XP, sounds, events and menu refresh.
 if(!job.active || job.selection.session!=epoch.load())return;
 nextStep=Clock::now()+std::chrono::milliseconds(250);
 status("Crafting "+std::to_string(job.completed+1)+" / "+std::to_string(job.requested)+"...");
}
void advance(RE::CraftingMenu* menu,float interval,std::uint32_t time) {
 originalAdvance(menu,interval,time);
 tick(menu);
}
void controls() {
 namespace ui=ImGuiMCP;Snapshot s;{std::lock_guard lock(gate);s=shared;}
 ui::TextWrapped("Highlight a recipe at a cooking pot or forge, then press the batch hotkey. Press it again during a batch to cancel.");
 ui::TextUnformatted(("Hotkey: DirectInput code "+std::to_string(hotkey)+" (default F8). Set Hotkey in BatchCrafting.ini to change it.").c_str());
 if(ui::Button("Read selected recipe"))requestOpen();
 ui::Separator();ui::TextUnformatted(s.name.c_str());ui::TextUnformatted(s.status.c_str());
 if(s.running){if(ui::Button("Cancel batch"))cancel=true;return;}
 ui::TextUnformatted(("Available: "+std::to_string(s.max)+" crafts; each makes "+std::to_string(s.selection.units)+" item(s).").c_str());
 ui::TextWrapped("Quantities mean recipe executions. For example, 10 crafts of a 24-arrow recipe make 240 arrows. Maximum is capped at 1000 crafts per batch.");
 for(auto n:{1u,5u,10u}){
  ui::BeginDisabled(s.max<n);if(ui::Button(("Craft "+std::to_string(n)).c_str()))requestStart(s,n);ui::EndDisabled();ui::SameLine();
 }
 ui::BeginDisabled(!s.max);if(ui::Button("Craft maximum"))requestStart(s,s.max);ui::EndDisabled();
 static int quantity=1;ui::InputInt("Custom quantity",&quantity);
 ui::BeginDisabled(quantity<1 || static_cast<unsigned>(quantity)>s.max);
 if(ui::Button("Craft custom quantity"))requestStart(s,static_cast<std::uint32_t>(quantity));ui::EndDisabled();
}
void __stdcall renderPage(){controls();}
void __stdcall renderWindow(){
 bool open=true;
 if(ImGuiMCP::Begin("Batch Crafting",&open,ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize))controls();
 ImGuiMCP::End();
 if(!open && window)window->IsOpen=false;
}
void registerMenu(){
 if(menuReady)return;auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");if(!module)return;
 for(auto name:{"AddWindow","AddSectionItem","GetMainWindow","igBegin","igEnd","igTextUnformatted","igTextWrappedV","igButton","igSameLine","igSeparator","igBeginDisabled","igEndDisabled","igInputInt"})
  if(!GetProcAddress(module,name)){SKSE::log::error("Missing Menu Framework export {}",name);return;}
 window=SKSEMenuFramework::AddWindow(renderWindow);
 if(!window)return;
 SKSEMenuFramework::SetSection("Batch Crafting");SKSEMenuFramework::AddSectionItem("Cooking and Smithing",renderPage);menuReady=true;
}
class Events final:public RE::BSTEventSink<RE::MenuOpenCloseEvent>,public RE::BSTEventSink<RE::InputEvent*> {
 RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* e,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
  if(e && e->menuName==RE::CraftingMenu::MENU_NAME){
   std::lock_guard controlLock(controller);
   ++epoch;cancel=true;job.stop();if(window)window->IsOpen=false;
   std::lock_guard lock(gate);shared={};
  }return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
  if(!events || !menuReady)return RE::BSEventNotifyControl::kContinue;
  for(auto* e=*events;e;e=e->next){auto* b=e->AsButtonEvent();
   if(b && b->device==RE::INPUT_DEVICE::kKeyboard && b->GetIDCode()==hotkey && b->IsDown() && unobstructed())requestOpen();
  }return RE::BSEventNotifyControl::kContinue;
 }
} events;
void message(SKSE::MessagingInterface::Message* e){
 if(e->type==SKSE::MessagingInterface::kPostLoad)registerMenu();
 if(e->type==SKSE::MessagingInterface::kDataLoaded){
  registerMenu();if(!menuReady){SKSE::log::error("SKSE Menu Framework 3.x is required; batch crafting disabled");return;}
  craft=REL::Relocation<void (*)(Sub*)>{REL::ID(51369)};
  REL::Relocation<std::uintptr_t> table{RE::VTABLE_CraftingMenu[0]};originalAdvance=table.write_vfunc(0x5,advance);
  RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
  RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&events);
  SKSE::log::info("Ready: hotkey={}, native craft=51369, cap=1000, menu-only controller",hotkey);
 }
 if(e->type==SKSE::MessagingInterface::kPreLoadGame || e->type==SKSE::MessagingInterface::kNewGame){std::lock_guard controlLock(controller);++epoch;cancel=true;job.stop();if(window)window->IsOpen=false;std::lock_guard lock(gate);shared={};}
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
 SKSE::PluginVersionData d{};d.PluginVersion({0,1,0,0});d.PluginName("BatchCrafting");d.AuthorName("Physics-helper contributors");
 d.UsesAddressLibrary(true);d.UsesStructsPost629(true);d.CompatibleVersions({REL::Version{1,6,1170,0}});return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse){
 if(skse->RuntimeVersion()!=REL::Version{1,6,1170,0})return false;
 auto path=SKSE::log::log_directory();if(!path)return false;*path/="BatchCrafting.log";
 spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true)));
 spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);SKSE::Init(skse);
 std::ifstream config("Data/SKSE/Plugins/BatchCrafting.ini");std::string line;
 while(std::getline(config,line))if(line.starts_with("Hotkey=")){try {const int k=std::stoi(line.substr(7));if(k>=2&&k<=211)hotkey=static_cast<std::uint32_t>(k);}catch(...) {}}
 SKSE::log::info("Batch Crafting 0.1.0 beta; Steam 1.6.1170");
 return SKSE::GetMessagingInterface()->RegisterListener(message);
}
