#include "Blacklist.h"
#include "BlacklistCore.h"
#include "Storage.h"
#include "SKSEMenuFramework.h"
#include <mutex>

namespace phial::blacklist {
namespace {
constexpr auto originalMod="The White Phial - Tweaks and Enhancements.esp";
constexpr auto decantMod="White Phial - Decanting.esp";
const std::filesystem::path path="Data/SKSE/Plugins/WhitePhialBlacklist.cfg";
struct Row { std::string key, label, source, search; };
std::mutex gate;
Rules rules;
std::shared_ptr<const std::vector<Row>> catalog=std::make_shared<const std::vector<Row>>();
Row current;
std::string status="Load a save to edit the blacklist.";
bool configOK=true, session=false, refreshed=false, pending=false, registered=false;
std::uint64_t epoch=0;

template<class T> T* form(std::uint32_t id,const char* mod) {
    auto* data=RE::TESDataHandler::GetSingleton(); return data ? data->LookupForm<T>(id,mod) : nullptr;
}
storage::Ref ref(RE::TESForm* f) {
    if (!f || (f->GetFormID()>>24)==0xFF || !f->GetFile(0)) throw storage::Error("Liquid has no persistent form identity");
    auto* file=f->GetFile(0);
    return {std::string(file->GetFilename()),f->GetFormID() & (file->IsLight()?0xFFFu:0xFFFFFFu)};
}
RE::AlchemyItem* unwrap(RE::AlchemyItem* potion) {
    if (!potion) return nullptr;
    if (potion==form<RE::AlchemyItem>(0x807,originalMod) || potion==form<RE::AlchemyItem>(0x80C,originalMod)) {
        auto* list=form<RE::BGSListForm>(0xD4A,originalMod);
        RE::TESForm* source=nullptr;
        if (list) list->ForEachForm([&](RE::TESForm* f) { source=f; return RE::BSContainer::ForEachResult::kStop; });
        auto* result=source ? source->As<RE::AlchemyItem>() : nullptr;
        if (!result || result==potion || result==form<RE::AlchemyItem>(0x807,originalMod) || result==form<RE::AlchemyItem>(0x80C,originalMod))
            throw storage::Error("The selected custom liquid is unavailable");
        return result;
    }
    if (potion->GetFormID()>=0x102019 && potion->GetFormID()<=0x10201E)
        return form<RE::AlchemyItem>(0x810+potion->GetFormID()-0x102019,decantMod);
    return potion;
}
std::string identity(RE::AlchemyItem* p) {
    p=unwrap(p); if (!p) throw storage::Error("No liquid selected");
    if (auto l=storage::protectedDefinition(p)) return recipeKey((l->flags & (1u<<17))!=0,l->effects);
    if ((p->GetFormID()>>24)!=0xFF) return staticKey(ref(p));
    std::vector<storage::Effect> effects;
    for (auto* e : p->effects) {
        if (!e) throw storage::Error("Missing potion effect");
        effects.push_back({ref(e->baseEffect),e->effectItem.magnitude,0,e->effectItem.area,e->effectItem.duration});
    }
    return recipeKey(p->IsPoison(),effects);
}
RE::BSTSmartPointer<RE::BSScript::Object> questObject() {
    RE::BSTSmartPointer<RE::BSScript::Object> object;
    auto* quest=form<RE::TESQuest>(0x1010AA,"Skyrim.esm");
    auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    auto* policy=vm ? vm->GetObjectHandlePolicy() : nullptr;
    if (quest && policy) vm->FindBoundObject(policy->GetHandleForObject(RE::FormType::Quest,quest),"MS12PostQuestScript",object);
    return object;
}
RE::AlchemyItem* selected() {
    auto object=questObject(); auto* variable=object ? object->GetProperty("Replicated") : nullptr;
    return variable && variable->IsObject() ? variable->Unpack<RE::AlchemyItem*>() : nullptr;
}
void resume() {
    auto object=questObject(); auto* vm=RE::BSScript::Internal::VirtualMachine::GetSingleton();
    RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
    if (object && vm) vm->DispatchMethodCall(object,"WPD_ResumeAfterBlacklistChange",RE::MakeFunctionArguments(),callback);
}
bool inGame() {
    auto* ui=RE::UI::GetSingleton();
    return ui && RE::PlayerCharacter::GetSingleton() && !ui->IsMenuOpen(RE::MainMenu::MENU_NAME) && !ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
}
bool isBlocked(RE::StaticFunctionTag*,RE::AlchemyItem* potion) {
    try {
        auto key=identity(potion); std::lock_guard lock(gate);
        return !configOK || rules.contains(key);
    } catch (const std::exception& e) {
        SKSE::log::warn("Blacklist refused unresolved liquid: {}",e.what()); return true;
    }
}
Row row(RE::AlchemyItem* p,const char* display=nullptr) {
    auto key=identity(p); auto* liquid=unwrap(p);
    std::string label=display && *display ? display : (liquid->GetName() ? liquid->GetName() : "");
    if (label.empty()) throw storage::Error("Unnamed liquid");
    auto source=key.starts_with("R:") ? std::string("Custom recipe") : std::string(liquid->GetFile(0)->GetFilename());
    return {key,label,source,lower(label+" "+source)};
}
void refresh() {
    std::uint64_t ticket;
    { std::lock_guard lock(gate); if (!session || pending) return; pending=true; ticket=epoch; }
    SKSE::GetTaskInterface()->AddTask([ticket] {
        { std::lock_guard lock(gate); if (ticket!=epoch || !session) return; }
        if (!inGame()) { std::lock_guard lock(gate); if(ticket==epoch) pending=false; return; }
        std::map<std::string,Row> found;
        auto* phials=form<RE::BGSListForm>(0x817,originalMod);
        auto add=[&](RE::AlchemyItem* p,const char* name=nullptr) {
            if (!p || p->IsFood() || (phials && phials->HasForm(p)) || p==form<RE::AlchemyItem>(0x80A,originalMod)) return;
            try { auto r=row(p,name); found.insert_or_assign(r.key,std::move(r)); } catch (const std::exception&) {}
        };
        if (auto* data=RE::TESDataHandler::GetSingleton()) for (auto* p : data->GetFormArray<RE::AlchemyItem>()) add(p);
        // Borrow inventory entries only inside this game-thread snapshot.
        auto inventory=RE::PlayerCharacter::GetSingleton()->GetInventory([](RE::TESBoundObject& o) { return o.Is(RE::FormType::AlchemyItem); });
        for (auto& [object,entry] : inventory) if (entry.first>0 && entry.second) add(object->As<RE::AlchemyItem>(),entry.second->GetDisplayName());
        Row chosen;
        try { if (auto* p=selected()) { chosen=row(p); found.insert_or_assign(chosen.key,chosen); } } catch (const std::exception&) {}
        auto rows=std::make_shared<std::vector<Row>>();
        for (auto& [key,r] : found) rows->push_back(std::move(r));
        std::sort(rows->begin(),rows->end(),[](const Row& a,const Row& b) { return a.search<b.search; });
        std::lock_guard lock(gate);
        if (ticket!=epoch || !session) return;
        catalog=std::move(rows); current=std::move(chosen); pending=false; refreshed=true;
        if (configOK) status="Ready. Changes save automatically; no background inventory polling.";
    });
}
void change(Row r,bool remove) {
    std::uint64_t ticket;
    { std::lock_guard lock(gate); if (!session || !configOK) return; ticket=epoch; }
    SKSE::GetTaskInterface()->AddTask([ticket,r=std::move(r),remove] {
        { std::lock_guard lock(gate); if (ticket!=epoch || !session) return; }
        if (!inGame()) return;
        auto* busy=form<RE::TESGlobal>(0x806,originalMod);
        if (!busy || busy->value!=0) {
            std::lock_guard lock(gate); status="Wait until the phial has finished changing its contents."; return;
        }
        std::string selectedKey;
        try { if (auto* p=selected()) selectedKey=identity(p); } catch (const std::exception&) {}
        bool restart=false;
        {
            std::lock_guard lock(gate);
            if (ticket!=epoch || !session || !configOK) return;
            try {
                Rules next=rules;
                if (remove) next.erase(r.key); else next.insert_or_assign(r.key,r.label);
                save(path,next); // Commit policy only after atomic file replacement succeeds.
                restart=remove && rules.contains(r.key) && r.key==selectedKey;
                rules=std::move(next); status=remove ? "Removed. The selected empty phial will restart its refill countdown." : "Blocked: "+r.label;
                SKSE::log::info("Blacklist {}: {}",remove?"removed":"added",r.label);
            } catch (const std::exception& e) { status=std::string("Rules unchanged: ")+e.what(); }
        }
        if (restart) resume();
    });
}
void __stdcall render() {
    namespace ui=ImGuiMCP;
    static char search[256]{};
    Rules snapshot; Row chosen; std::shared_ptr<const std::vector<Row>> rows;
    std::string message; bool active,needRefresh,waiting,ok;
    { std::lock_guard lock(gate); snapshot=rules; chosen=current; rows=catalog; message=status;
      active=session; needRefresh=session&&!refreshed&&!pending; waiting=pending; ok=configOK; }
    if (needRefresh) refresh();
    ui::TextWrapped("Blocked liquids cannot be assigned, refilled or decanted (including daily decanting). An already full phial can still be used once.");
    ui::TextWrapped("Custom recipes match by effects and strength, regardless of name. Rules are shared by saves using this configuration.");
    ui::TextUnformatted(message.c_str());
    if (!active) return;
    ui::BeginDisabled(waiting);
    if (ui::Button("Refresh potions and current contents")) refresh();
    ui::EndDisabled();
    ui::BeginDisabled(!ok);
    if (!chosen.key.empty()) {
        ui::TextUnformatted(("Current contents: "+chosen.label).c_str());
        if (snapshot.contains(chosen.key)) { if (ui::Button("Unblock current contents")) change(chosen,true); }
        else if (ui::Button("Block current contents")) change(chosen,false);
    }
    ui::Separator();
    ui::TextUnformatted(("Blacklist ("+std::to_string(snapshot.size())+")").c_str());
    if (ui::BeginChild("Blocked liquids",ImGuiMCP::ImVec2(0,180))) {
        std::size_t n=0;
        for (const auto& [key,label] : snapshot) {
            if (ui::Button(("Remove##blocked"+std::to_string(n++)).c_str())) change({key,label,{},{}},true);
            ui::SameLine(); ui::TextUnformatted(label.c_str());
        }
    }
    ui::EndChild();
    ui::InputText("Search name or source plugin",search,sizeof(search));
    const auto query=lower(search); std::size_t matches=0,shown=0;
    if (ui::BeginChild("Available liquids",ImGuiMCP::ImVec2(0,300))) {
        for (const auto& r : *rows) {
            if (!query.empty() && r.search.find(query)==r.search.npos) continue;
            ++matches; if(shown>=100) continue; ++shown;
            const bool blocked=snapshot.contains(r.key);
            if (ui::Button(((blocked?"Remove##result":"Add##result")+std::to_string(shown)).c_str())) change(r,blocked);
            ui::SameLine(); ui::TextUnformatted((r.label+"  ["+r.source+"]").c_str());
        }
    }
    ui::EndChild();
    ui::TextUnformatted((std::to_string(matches)+" matches. Showing up to 100; refine the search if needed.").c_str());
    ui::EndDisabled();
}
void registerMenu() {
    if (registered) return;
    auto module=GetModuleHandleW(L"SKSEMenuFramework.dll");
    if (!module) return;
    for (auto name : {"AddSectionItem","igTextWrappedV","igTextUnformatted","igButton","igSameLine","igSeparator","igBeginChild_Str","igEndChild","igInputText","igBeginDisabled","igEndDisabled"})
        if (!GetProcAddress(module,name)) { SKSE::log::warn("Blacklist menu missing export {}",name); return; }
    SKSEMenuFramework::SetSection("White Phial"); SKSEMenuFramework::AddSectionItem("Blacklist",render); registered=true;
}
}
bool registerPapyrus(RE::BSScript::IVirtualMachine* vm) {
    vm->RegisterFunction("IsBlockedNative","WPD_Blacklist",isBlocked); return true;
}
void message(SKSE::MessagingInterface::Message* event) {
    using M=SKSE::MessagingInterface;
    if (event->type==M::kPostLoad || event->type==M::kDataLoaded) registerMenu();
    if (event->type==M::kDataLoaded) {
        std::lock_guard lock(gate);
        try { rules=load(path); configOK=true; SKSE::log::info("Loaded {} blacklist rules",rules.size()); }
        catch (const std::exception& e) { configOK=false; status=std::string("Blacklist file error; refill/decant paused: ")+e.what(); SKSE::log::error("{}",status); }
    }
    if (event->type==M::kPreLoadGame || event->type==M::kPostLoadGame || event->type==M::kNewGame) {
        std::lock_guard lock(gate); ++epoch;
        session=event->type==M::kNewGame || (event->type==M::kPostLoadGame && event->data!=nullptr);
        refreshed=false; pending=false; current={}; catalog=std::make_shared<const std::vector<Row>>();
        if (configOK) status=session?"Open this page to refresh potions.":"Load a save to edit the blacklist.";
    }
}
}
