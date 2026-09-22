#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include "SKSEMenuFramework.h"
#include "Batch.h"
#include "SCIE.h"
#include "NativeBatch.h"
#include <atomic>
#include <fstream>
#include <mutex>

namespace {
using Sub=RE::CraftingSubMenus::ConstructibleObjectMenu;
// Yes Im Sure NG documents AE 51369 as the confirmed-craft routine.
REL::Relocation<void (*)(Sub*)> craft;
SKSEMenuFramework::Model::WindowInterface* window=nullptr;
std::atomic<std::uint64_t> epoch{0};
std::atomic<bool> queued{false};
std::uint32_t hotkey=66;
bool menuReady=false,faulted=false;
std::recursive_mutex controller;
struct Snapshot {
 batch::Selection selection{};
 std::string name,status="Highlight a cooking or forge recipe, then press F8.";
 std::uint32_t max{};
 bool sharedInventory=false;
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
std::int64_t playerCount(RE::TESBoundObject* object) {
 auto* player=RE::PlayerCharacter::GetSingleton();if(!player || !object)return -1;
 auto inventory=player->GetInventoryCounts([object](RE::TESBoundObject& o){return &o==object;});
 auto it=inventory.find(object);return it==inventory.end()?0:it->second;
}
struct Inspection {
 std::vector<batch::Material> materials;
 std::map<std::uint32_t,RE::TESBoundObject*> objects;
 std::uint32_t max{};
 scie::Mode mode=scie::Mode::Player;
 std::string reason;
};
Inspection inspect(Sub* sub,RE::BGSConstructibleObject* r) {
 Inspection out;
 auto* player=RE::PlayerCharacter::GetSingleton();
 auto* output=r && r->createdItem ? r->createdItem->As<RE::TESBoundObject>():nullptr;
 if(faulted || !nativebatch::ready){out.reason="Bulk crafting is disabled. See BatchCrafting.log.";return out;}
 if(!player || !sub || !r || !output || !r->data.numConstructed){out.reason="No valid recipe selected.";return out;}
 RE::GFxValue entry,enabled,id;
 if(!sub->itemList.GetMember("selectedEntry",&entry) || !entry.IsObject() ||
    !entry.GetMember("enabled",&enabled) || !enabled.IsBool() || !enabled.GetBool()) {
  out.reason="The selected recipe is unavailable, or the crafting interface is unsupported.";return out;
 }
 if(entry.GetMember("formId",&id) && id.IsNumber() && id.GetUInt()!=output->GetFormID()) {
  out.reason="The highlighted item and native recipe do not match. Select the recipe again.";return out;
 }
 out.mode=scie::mode();
 if(out.mode==scie::Mode::Unavailable){out.reason="SCIE is installed but its crafting session could not be verified. Reopen the crafting station.";return out;}
 if(!r->conditions.IsTrue(player,player)){out.reason="The recipe requirements are not met.";return out;}
 const auto& requirements=r->requiredItems;
 if(!requirements.containerObjects || !requirements.numContainerObjects || requirements.numContainerObjects>256) {
  out.reason="This recipe has no supported material requirements.";return out;
 }
 // Read the player inventory once only when SCIE is absent. The SCIE count
 // already includes it, so NEVER add the two counts together.
 const auto inventory=out.mode==scie::Mode::Player ? player->GetInventoryCounts():RE::TESObjectREFR::InventoryCountMap{};
 for(std::uint32_t i=0;i<requirements.numContainerObjects;++i) {
  auto* m=requirements.containerObjects[i];
  if(!m || !m->obj){out.reason="Invalid recipe material.";return out;}
  std::int64_t amount;
  if(out.mode==scie::Mode::Shared)amount=scie::sharedCount(m->obj);
  else {auto it=inventory.find(m->obj);amount=it==inventory.end()?0:it->second;}
  out.materials.push_back({m->obj->GetFormID(),m->count,amount});out.objects[m->obj->GetFormID()]=m->obj;
 }
 out.max=batch::maximum(out.materials,output->GetFormID(),r->data.numConstructed);
 if(!out.max)out.reason="Insufficient materials, or a free/self-producing recipe that cannot be batched.";
 return out;
}
void status(const std::string& text,bool notify=false) {
 {std::lock_guard lock(gate);shared.status=text;}
 if(notify)RE::DebugNotification(text.c_str());
}
void requestOpen() {
 const auto ticket=epoch.load();
 SKSE::GetTaskInterface()->AddUITask([ticket] {
  std::lock_guard controlLock(controller);
  if(queued.load() || ticket!=epoch.load() || !unobstructed())return;
  auto menu=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();auto* sub=submenu(menu.get());
  if(!sub){RE::DebugNotification("Batch Crafting supports cooking and making items, not alchemy, enchanting or tempering.");return;}
  auto* r=recipe(sub);Snapshot s;s.selection=selection(menu.get(),sub,r);
  s.name=r&&r->createdItem&&r->createdItem->GetName()?r->createdItem->GetName():"No recipe";
  auto check=inspect(sub,r);s.max=check.max;s.sharedInventory=check.mode==scie::Mode::Shared;
  s.status=s.max?"Choose the quantity to craft in one operation.":check.reason;
  SKSE::log::info("Preview: recipe={:08X}, max={}, SCIE={}, status={}",s.selection.recipe,s.max,s.sharedInventory,s.status);
  {std::lock_guard lock(gate);shared=std::move(s);}
  if(window)window->IsOpen=true;
 });
}
void requestStart(const Snapshot& s,std::uint32_t n) {
 if(queued.exchange(true))return;
 SKSE::GetTaskInterface()->AddUITask([s,n] {
  struct ClearQueue { ~ClearQueue(){queued=false;} } clear;
  std::lock_guard controlLock(controller);
  if(s.selection.session!=epoch.load() || !unobstructed())return;
  auto menu=RE::UI::GetSingleton()->GetMenu<RE::CraftingMenu>();auto* sub=submenu(menu.get());auto* r=recipe(sub);
  if(selection(menu.get(),sub,r)!=s.selection){status("Selection changed. Refresh the selected recipe.");return;}
  auto check=inspect(sub,r);
  auto t=check.max ? batch::Transaction::create(s.selection,n,check.materials):std::nullopt;
  if(!t){status(check.reason.empty()?"That quantity is no longer available. Refresh the selected recipe.":check.reason);return;}
  auto* output=r->createdItem->As<RE::TESBoundObject>();const auto before=playerCount(output);
  if(before<0 || before>batch::engineLimit-t->totalOutput()){status("Output count is outside the supported range.");return;}
  if(window)window->IsOpen=false;
  if(auto* main=SKSEMenuFramework::GetMainWindow())main->IsOpen=false;
  nativebatch::Context context{std::move(*t)};
  SKSE::log::info("Bulk begin: recipe={:08X}, quantity={}, total output={}, SCIE={}",s.selection.recipe,n,context.transaction.totalOutput(),check.mode==scie::Mode::Shared);
  {
   batch::Scope<nativebatch::Context> scope(nativebatch::active,&context);
   craft(sub); // Exactly ONE native craft, irrespective of selected quantity.
  }
  const auto after=playerCount(output);
  auto& transaction=context.transaction;
  const bool sessionStillOpen=unobstructed() && epoch.load()==s.selection.session;
  bool verified=sessionStillOpen && transaction.accounted() && after-before>=transaction.totalOutput();
  // Verify the merged inventory debit after SCIE refreshes its own cache.
  // There is no automatic retry or guessed refund on an unexpected hook path.
  for(const auto& [id,cost]:transaction.costs) {
   const auto now=check.mode==scie::Mode::Shared ? (sessionStillOpen ? scie::sharedCount(check.objects.at(id)):-1):playerCount(check.objects.at(id));
   const auto expected=cost.each*n;
   SKSE::log::info("Material {:08X}: before={}, after={}, expected debit={}, scaled base={}/{}",id,cost.available,now,expected,cost.seen,cost.each);
   if(now<0 || cost.available-now<expected)verified=false;
  }
  SKSE::log::info("Bulk result: output delta={}, expected={}, removal calls={}, output calls={}, skill calls={}, native craft events={}, verified={}",after-before,transaction.totalOutput(),transaction.removalCalls,transaction.outputCalls,transaction.skillCalls,context.eventCount,verified);
  if(!verified) {
   faulted=true;{std::lock_guard lock(gate);shared.max=0;}
   status("Batch accounting mismatch. Further batches disabled; please provide BatchCrafting.log.",true);return;
  }
  nativebatch::completeEvents(context);
  status("Crafted "+std::to_string(transaction.totalOutput())+" item(s) in one batch.",true);
  // Refresh our preview after the transaction; no pointers are retained in UI.
  if(unobstructed() && epoch.load()==s.selection.session && submenu(menu.get())==sub && recipe(sub)==r) {
   auto refreshed=inspect(sub,r);std::lock_guard lock(gate);shared.max=refreshed.max;
  }
 });
}
void controls() {
 namespace ui=ImGuiMCP;Snapshot s;{std::lock_guard lock(gate);s=shared;}
 ui::TextWrapped("Highlight a cooking or forge recipe and press F8. Choose a quantity to craft it together in one operation.");
 ui::TextUnformatted(("Hotkey: DirectInput code "+std::to_string(hotkey)+" (default F8). Change it in BatchCrafting.ini.").c_str());
 if(ui::Button("Read selected recipe"))requestOpen();
 ui::Separator();ui::TextUnformatted(s.name.c_str());ui::TextUnformatted(s.status.c_str());
 ui::TextUnformatted(s.sharedInventory?"Materials: player + SCIE's active crafting sources":"Materials: player inventory");
 ui::TextUnformatted(("Available: "+std::to_string(s.max)+" recipe units; each produces "+std::to_string(s.selection.units)+" item(s).").c_str());
 ui::TextWrapped("10 units of a 24-arrow recipe produce 240 arrows. Maximum is capped at 1000 recipe units per batch. Close this window to cancel before confirming.");
 ui::BeginDisabled(queued.load());
 for(auto n:{1u,5u,10u}){
  ui::BeginDisabled(s.max<n);if(ui::Button(("Craft "+std::to_string(n)).c_str()))requestStart(s,n);ui::EndDisabled();ui::SameLine();
 }
 ui::BeginDisabled(!s.max);if(ui::Button("Craft maximum"))requestStart(s,s.max);ui::EndDisabled();
 static int quantity=1;ui::InputInt("Custom quantity",&quantity);
 if(quantity>0)ui::TextUnformatted(("Total output: "+std::to_string(static_cast<std::uint64_t>(quantity)*s.selection.units)+" item(s)").c_str());
 ui::BeginDisabled(quantity<1 || static_cast<unsigned>(quantity)>s.max);
 if(ui::Button("Craft custom quantity"))requestStart(s,static_cast<std::uint32_t>(quantity));ui::EndDisabled();
 ui::EndDisabled();
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
   ++epoch;if(window)window->IsOpen=false;
   std::lock_guard lock(gate);shared={};
  }return RE::BSEventNotifyControl::kContinue;
 }
 RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* events,RE::BSTEventSource<RE::InputEvent*>*) override {
  if(!events || !menuReady)return RE::BSEventNotifyControl::kContinue;
  for(auto* e=*events;e;e=e->next){auto* b=e->AsButtonEvent();
   if(b && b->device==RE::INPUT_DEVICE::kKeyboard && b->GetIDCode()==hotkey && b->IsDown() && unobstructed()) {
    if(window && window->IsOpen.load())window->IsOpen=false;else requestOpen();
   }
  }return RE::BSEventNotifyControl::kContinue;
 }
} events;
void message(SKSE::MessagingInterface::Message* e){
 if(e->type==SKSE::MessagingInterface::kPostLoad)registerMenu();
 if(e->type==SKSE::MessagingInterface::kDataLoaded){
  registerMenu();if(!menuReady){SKSE::log::error("SKSE Menu Framework 3.x is required; batch crafting disabled");return;}
  craft=REL::Relocation<void (*)(Sub*)>{REL::ID(51369)};
  scie::connect();
  if(!nativebatch::install()){SKSE::log::error("Bulk hooks could not be installed; batch crafting disabled");return;}
  RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&events);
  RE::BSInputDeviceManager::GetSingleton()->AddEventSink(&events);
  SKSE::log::info("Ready: hotkey={}, native craft=51369, cap=1000, single native call, SCIE-aware counts",hotkey);
 }
 if(e->type==SKSE::MessagingInterface::kPreLoadGame || e->type==SKSE::MessagingInterface::kNewGame){std::lock_guard controlLock(controller);++epoch;if(window)window->IsOpen=false;std::lock_guard lock(gate);shared={};}
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
 SKSE::PluginVersionData d{};d.PluginVersion({0,2,0,0});d.PluginName("BatchCrafting");d.AuthorName("Physics-helper contributors");
 d.UsesAddressLibrary(true);d.UsesStructsPost629(true);d.CompatibleVersions({REL::Version{1,6,1170,0}});return d;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse){
 if(skse->RuntimeVersion()!=REL::Version{1,6,1170,0})return false;
 auto path=SKSE::log::log_directory();if(!path)return false;*path/="BatchCrafting.log";
 spdlog::set_default_logger(std::make_shared<spdlog::logger>("global",std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true)));
 spdlog::set_level(spdlog::level::info);spdlog::flush_on(spdlog::level::info);SKSE::Init(skse);
 std::ifstream config("Data/SKSE/Plugins/BatchCrafting.ini");std::string line;
 while(std::getline(config,line))if(line.starts_with("Hotkey=")){try {const int k=std::stoi(line.substr(7));if(k>=2&&k<=211)hotkey=static_cast<std::uint32_t>(k);}catch(...) {}}
 SKSE::log::info("Batch Crafting 0.2.0 beta; Steam 1.6.1170");
 return SKSE::GetMessagingInterface()->RegisterListener(message);
}
