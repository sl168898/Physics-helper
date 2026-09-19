#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "TradeRules.h"

namespace {
using Ref = RE::TESObjectREFR;
using Object = RE::TESBoundObject;
using Extra = RE::ExtraDataList;
struct Signature {
    std::string name;
    RE::FormID enchant{}, poison{};
    float health{1}, charge{-1};
    std::uint32_t poisonCount{};
    std::uint16_t capacity{};
    unsigned soul{};
    bool removeOnUnequip{};
    bool operator==(const Signature&) const = default;
};
Signature signature(Object* object, Extra* extra) {
    Signature s;
    const char* name = extra ? extra->GetDisplayName(object) : object->GetName();
    s.name = name ? name : "Item";
    if (!extra) return s;
    if (auto* e=extra->GetByType<RE::ExtraHealth>()) s.health=e->health;
    if (auto* e=extra->GetByType<RE::ExtraCharge>()) s.charge=e->charge;
    if (auto* e=extra->GetByType<RE::ExtraEnchantment>()) {
        s.enchant=e->enchantment ? e->enchantment->GetFormID() : 0;
        s.capacity=e->charge; s.removeOnUnequip=e->removeOnUnequip;
    }
    if (auto* e=extra->GetByType<RE::ExtraPoison>()) {
        s.poison=e->poison ? e->poison->GetFormID() : 0; s.poisonCount=e->count;
    }
    s.soul=static_cast<unsigned>(extra->GetSoulLevel());
    return s;
}
struct Stack {
    // Identity is compared with current live inventory before dereferencing.
    std::uintptr_t identity{};
    std::int32_t count{};
    Signature value;
};
struct Offer {
    barter::Line rule;
    RE::RefHandle owner{};
    Object* object{};
    std::string name;
    std::int32_t baseValue{};
    std::vector<Stack> stacks;
};
struct State {
    std::mutex mutex;
    RE::GFxMovieView* movie{}; // comparison only; the UI owns this pointer
    RE::RefHandle merchant{};
    std::uint64_t nextId{1};
    bool busy{}, closed{};
    std::vector<Offer> offers;
    std::string status;
    std::int64_t playerGold{}, vendorGold{};
};
std::mutex activeMutex;
std::weak_ptr<State> active;

Object* gold() { return RE::TESForm::LookupByID<Object>(0xF); }
std::int64_t count(Ref* ref, Object* object) {
    if (!ref || !object) return 0;
    const auto inv=ref->GetInventoryCounts([&](Object& o){return &o==object;});
    auto it=inv.find(object);
    return it==inv.end() ? 0 : std::max<std::int64_t>(0,it->second);
}
RE::GPtr<RE::BarterMenu> current(const State& state) {
    auto* ui=RE::UI::GetSingleton();
    auto menu=ui ? ui->GetMenu<RE::BarterMenu>() : nullptr;
    if (!menu || menu->uiMovie.get()!=state.movie || state.closed) return nullptr;
    if (state.merchant && state.merchant!=RE::BarterMenu::GetTargetRefHandle()) return nullptr;
    return menu;
}
std::vector<barter::Line> rules(const State& state) {
    std::vector<barter::Line> result;
    for (auto& row:state.offers) result.push_back(row.rule);
    return result;
}
std::vector<Stack> liveStacks(Ref* source, Object* object) {
    std::vector<Stack> result;
    auto inv=source->GetInventory([&](Object& o){return &o==object;});
    const auto it=inv.find(object);
    if(it==inv.end() || it->second.first<=0) return result;
    std::int64_t plain=it->second.first;
    const auto* entry=it->second.second.get();
    if(entry && entry->extraLists) for(auto* x:*entry->extraLists) if(x) {
        const auto n=std::max(0,x->GetCount());
        result.push_back({reinterpret_cast<std::uintptr_t>(x),n,signature(object,x)});
        plain-=n;
    }
    if(plain>0) result.push_back({0,static_cast<std::int32_t>(plain),signature(object,nullptr)});
    return result;
}
bool sameRow(const Offer& a, const Offer& b) {
    if(a.rule.buy!=b.rule.buy || a.owner!=b.owner || a.object!=b.object || a.stacks.size()!=b.stacks.size()) return false;
    for(std::size_t i=0;i<a.stacks.size();++i) if(a.stacks[i].identity!=b.stacks[i].identity) return false;
    return true;
}
bool matchInitial(Ref* source, const Offer& offer) {
    auto live=liveStacks(source,offer.object);
    std::int64_t available=0;
    for(const auto& wanted:offer.stacks) {
        auto found=std::ranges::find_if(live,[&](const Stack& x){return x.identity==wanted.identity && x.value==wanted.value;});
        if(found==live.end()) return false;
        available+=std::min(found->count,wanted.count);
    }
    return available>=offer.rule.count;
}
void stringMember(RE::GFxMovie* movie, RE::GFxValue& object, const char* name, const std::string& text) {
    RE::GFxValue value; movie->CreateString(&value,text.c_str()); object.SetMember(name,value);
}
void numberMember(RE::GFxValue& object,const char* name,double number) { object.SetMember(name,RE::GFxValue(number)); }
void reply(RE::GFxMovie* movie, State& state, RE::GFxValue* result) {
    movie->CreateObject(result);
    auto lines=rules(state);
    const auto total=barter::evaluate(lines,state.playerGold,state.vendorGold);
    result->SetMember("busy",RE::GFxValue(state.busy));
    result->SetMember("ready",RE::GFxValue(bool(total) && !state.busy));
    stringMember(movie,*result,"message",state.status.empty() ? std::string(barter::message(total.error)) : state.status);
    numberMember(*result,"buy",static_cast<double>(total.purchases));
    numberMember(*result,"sell",static_cast<double>(total.sales));
    numberMember(*result,"net",static_cast<double>(total.purchases-total.sales));
    numberMember(*result,"playerGold",static_cast<double>(state.playerGold));
    numberMember(*result,"vendorGold",static_cast<double>(state.vendorGold));
    RE::GFxValue entries; movie->CreateArray(&entries);
    for(const auto& row:state.offers) {
        RE::GFxValue entry; movie->CreateObject(&entry);
        numberMember(entry,"id",static_cast<double>(row.rule.id));
        numberMember(entry,"count",row.rule.count);
        numberMember(entry,"price",row.rule.price);
        entry.SetMember("buy",RE::GFxValue(row.rule.buy));
        stringMember(movie,entry,"name",row.name);
        entries.PushBack(entry);
    }
    result->SetMember("entries",entries);
}
void notify(const std::shared_ptr<State>& state) {
    SKSE::GetTaskInterface()->AddUITask([state]{
        std::scoped_lock lock(state->mutex);
        if(auto menu=current(*state)) {
            RE::GFxValue value; reply(menu->uiMovie.get(),*state,&value);
            menu->uiMovie->Invoke("_root.Menu_mc.BBResult",nullptr,&value,1);
        }
    });
}
struct Move {
    RE::NiPointer<Ref> source, destination;
    Object* object{};
    Stack stack;
    std::int32_t amount{}, moved{};
};
// Resolve the exact live instance first; a matching value is a fallback only
// after earlier moves in this same transaction may have merged identical stacks.
Extra* resolve(const Move& move, bool reverse, bool allowMerged, bool& found) {
    auto* source=reverse ? move.destination.get() : move.source.get();
    auto live=liveStacks(source,move.object);
    const auto needed=reverse ? move.moved : move.amount;
    auto it=std::ranges::find_if(live,[&](const Stack& x){return x.identity==move.stack.identity && x.value==move.stack.value && x.count>=needed;});
    if(it==live.end() && allowMerged) it=std::ranges::find_if(live,[&](const Stack& x){return x.value==move.stack.value && x.count>=needed;});
    found=it!=live.end();
    return found ? reinterpret_cast<Extra*>(it->identity) : nullptr;
}
struct Transaction {
    std::shared_ptr<State> state;
    RE::NiPointer<Ref> player, vendor, chest;
    std::vector<Move> items, payments;
    barter::Settlement settlement;
    double experience{};
    bool recoveryOK{true};
    std::string error;
    bool validate() {
        if(!current(*state)) { error="The barter menu closed; the offer was cancelled"; return false; }
        auto lines=rules(*state);
        const auto p=count(player.get(),gold());
        const auto actual=count(vendor.get(),gold())+(vendor.get()!=chest.get()?count(chest.get(),gold()):0);
        // A virtual or stale UI purse cannot authorize spending nonexistent gold.
        const auto v=std::min(state->vendorGold,actual);
        settlement=barter::evaluate(lines,p,v);
        if(!settlement) { error=barter::message(settlement.error); return false; }
        for(const auto& offer:state->offers) {
            auto source=RE::TESObjectREFR::LookupByHandle(offer.owner);
            if(!offer.rule.buy) source=player;
            if(!source || !matchInitial(source.get(),offer)) {
                // Some barter rows expose the actor handle for chest inventory.
                if(offer.rule.buy && matchInitial(chest.get(),offer)) source=chest;
                else { error=barter::message(barter::Error::changed); return false; }
            }
            auto live=liveStacks(source.get(),offer.object);
            auto remain=offer.rule.count;
            for(const auto& wanted:offer.stacks) {
                auto it=std::ranges::find_if(live,[&](const Stack& x){return x.identity==wanted.identity && x.value==wanted.value;});
                if(it==live.end()) {error=barter::message(barter::Error::changed); return false;}
                const auto n=std::min({remain,it->count,wanted.count});
                if(n>0) items.push_back({source,offer.rule.buy ? player : chest,offer.object,*it,n,0});
                remain-=n; if(!remain) break;
            }
            if(remain) {error=barter::message(barter::Error::changed);return false;}
            experience+=double(std::max(0,offer.baseValue))*offer.rule.count;
        }
        // Reject two UI rows that reserve the same underlying stack.
        for(std::size_t i=0;i<items.size();++i) {
            std::int64_t reserved=0;
            for(const auto& m:items) if(m.source.get()==items[i].source.get() && m.object==items[i].object && m.stack.identity==items[i].stack.identity) reserved+=m.amount;
            if(reserved>items[i].stack.count) {error=barter::message(barter::Error::changed);return false;}
        }
        auto addPayment=[&](RE::NiPointer<Ref> from, RE::NiPointer<Ref> to,std::int32_t n){
            if(n) payments.push_back({from,to,gold(),{0,n,signature(gold(),nullptr)},n,0});
        };
        if(settlement.net>0) addPayment(player,chest,static_cast<std::int32_t>(settlement.net));
        else if(settlement.net<0) {
            const auto n=-settlement.net;
            const auto actorPart=std::min(n,count(vendor.get(),gold()));
            addPayment(vendor,player,static_cast<std::int32_t>(actorPart));
            if(n>actorPart) addPayment(chest,player,static_cast<std::int32_t>(n-actorPart));
        }
        return true;
    }
    bool execute(Move& m, bool currency=false) {
        bool found=true;
        Extra* x=currency ? nullptr : resolve(m,false,true,found);
        if(!found) {error="An item changed during the exchange";return false;}
        const auto beforeSource=count(m.source.get(),m.object), beforeDest=count(m.destination.get(),m.object);
        m.source->RemoveItem(m.object,m.amount,RE::ITEM_REMOVE_REASON::kSelling,x,m.destination.get());
        const auto removed=beforeSource-count(m.source.get(),m.object);
        const auto added=count(m.destination.get(),m.object)-beforeDest;
        m.moved=static_cast<std::int32_t>(std::clamp<std::int64_t>(added,0,m.amount));
        if(removed!=m.amount || added!=m.amount) {
            error="An inventory hook interrupted the exchange";
            if(removed!=added) recoveryOK=false;
            SKSE::log::error("Transfer mismatch {:08X}: requested {}, removed {}, received {}",m.object->GetFormID(),m.amount,removed,added);
            return false;
        }
        return true;
    }
    bool reverse(Move& m, bool currency=false) {
        if(m.moved<=0) return true;
        bool found=true;
        Extra* x=currency ? nullptr : resolve(m,true,true,found);
        if(!found) {recoveryOK=false;return false;}
        const auto beforeSource=count(m.source.get(),m.object), beforeDest=count(m.destination.get(),m.object);
        m.destination->RemoveItem(m.object,m.moved,RE::ITEM_REMOVE_REASON::kSelling,x,m.source.get());
        bool ok=count(m.source.get(),m.object)-beforeSource==m.moved && beforeDest-count(m.destination.get(),m.object)==m.moved;
        recoveryOK &= ok;
        return ok;
    }
    bool move(std::size_t i) { return execute(items[i]); }
    void recoverFailed(std::size_t i) { reverse(items[i]); }
    void undo(std::size_t i) { reverse(items[i]); }
    bool pay() {for(auto& m:payments) if(!execute(m,true)) return false;return true;}
    void refund() {for(auto it=payments.rbegin();it!=payments.rend();++it) reverse(*it,true);}
};
void commit(const std::shared_ptr<State>& state) {
    SKSE::GetTaskInterface()->AddTask([state]{
        std::scoped_lock lock(state->mutex);
        if(!state->busy) return;
        auto menu=current(*state);
        auto* player=RE::PlayerCharacter::GetSingleton();
        auto vendor=Ref::LookupByHandle(state->merchant);
        auto* actor=vendor ? vendor->As<RE::Actor>() : nullptr;
        auto* faction=actor ? actor->GetVendorFaction() : nullptr;
        auto* chest=faction ? faction->vendorData.merchantContainer : nullptr;
        if(!chest) chest=vendor.get();
        if(!menu || !player || !vendor || !chest || !gold()) {
            state->status="Merchant unavailable; no exchange made"; state->busy=false; notify(state); return;
        }
        Transaction tx; tx.state=state; tx.player=RE::NiPointer<Ref>(player); tx.vendor=vendor; tx.chest=RE::NiPointer<Ref>(chest);
        // validate constructs the concrete item plan, so execute it only once.
        bool ok=tx.validate();
        if(ok) {
            struct Prepared {
                Transaction& tx;
                bool validate(){return true;}
                bool move(std::size_t i){return tx.move(i);}
                void recoverFailed(std::size_t i){tx.recoverFailed(i);}
                void undo(std::size_t i){tx.undo(i);}
                bool pay(){return tx.pay();}
                void refund(){tx.refund();}
            } prepared{tx};
            ok=barter::transact(prepared,tx.items.size());
        }
        if(ok) {
            // Use actual item values, including enchantments, and every unit.
            if(tx.experience>0) player->AddSkillExperience(RE::ActorValue::kSpeech,static_cast<float>(std::min(tx.experience,double(barter::limit))));
            state->vendorGold+=tx.settlement.net;
            state->playerGold=count(player,gold());
            state->offers.clear(); state->status="Exchange complete";
            SKSE::log::info("Exchange: buy {}, sell {}, net {}",tx.settlement.purchases,tx.settlement.sales,tx.settlement.net);
        } else {
            state->status=tx.error;
            if(!tx.recoveryOK) {
                state->status="Exchange interrupted. Reload your save; see BalancedBarter.log";
                SKSE::log::critical("Another inventory hook prevented complete recovery; reload the save.");
            }
            // Do not keep pointer identities across any attempted mutation.
            if(!tx.items.empty()) state->offers.clear();
        }
        for(auto* ref:{static_cast<Ref*>(player),vendor.get(),chest}) {
            if(auto* changes=ref->GetInventoryChanges()) changes->changed=true;
            ref->AddChange(Ref::ChangeFlags::kInventory);
            RE::SendUIMessage::SendInventoryUpdateMessage(ref,nullptr);
        }
        state->busy=false;
        notify(state);
    });
}
bool integer(const RE::GFxValue& value, std::int32_t& out) {
    if(!value.IsNumber()) return false;
    const auto n=value.GetNumber();
    if(!std::isfinite(n) || n<0 || n>barter::limit || n!=std::floor(n)) return false;
    out=static_cast<std::int32_t>(n); return true;
}
enum class Operation { open, queue, remove, clear, commit, close, status };
class Handler final:public RE::GFxFunctionHandler {
    std::shared_ptr<State> state;
    Operation operation;
public:
    Handler(std::shared_ptr<State> s,Operation op):state(std::move(s)),operation(op){}
    void Call(Params& p) override {
        std::scoped_lock lock(state->mutex);
        if(operation==Operation::close) { if(!state->busy) {state->closed=true;state->offers.clear();} return; }
        auto menu=current(*state);
        if(!menu) {state->status="Barter menu is not ready";reply(p.movie,*state,p.retVal);return;}
        if(!state->merchant) state->merchant=RE::BarterMenu::GetTargetRefHandle();
        if(!state->busy) {
            std::int32_t pg{},vg{};
            if(p.argCount>=2 && integer(p.args[p.argCount-2],pg) && integer(p.args[p.argCount-1],vg)) {
                state->playerGold=pg; state->vendorGold=vg;
            }
            if(operation==Operation::clear) {state->offers.clear();state->status="Offer cleared";}
            else if(operation==Operation::remove && p.argCount>=3) {
                std::int32_t id{};
                if(integer(p.args[0],id)) std::erase_if(state->offers,[&](const Offer& x){return x.rule.id==static_cast<unsigned>(id);});
                state->status.clear();
            } else if(operation==Operation::queue && p.argCount==5) {
                std::int32_t amount{},price{};
                auto* list=menu->GetRuntimeData().itemList;
                auto* selected=list ? list->GetSelectedItem() : nullptr;
                auto* desc=selected ? selected->data.objDesc : nullptr;
                const bool buy=p.args[2].IsBool() && p.args[2].GetBool();
                auto* player=RE::PlayerCharacter::GetSingleton();
                const bool belongsToPlayer=selected && player && selected->data.owner==player->GetHandle().native_handle();
                if(!integer(p.args[0],amount) || !amount || !integer(p.args[1],price) || !desc || !desc->object || desc->IsQuestObject() || desc->object==gold() || buy==belongsToPlayer || !selected->data.GetEnabled()) {
                    state->status="This item cannot be added to the offer";
                } else {
                    Offer offer; offer.owner=selected->data.owner; offer.object=desc->object;
                    const auto available=std::min<std::uint32_t>(selected->data.GetCount(),static_cast<std::uint32_t>(barter::limit));
                    offer.rule={state->nextId,buy,amount,price,static_cast<std::int32_t>(available)};
                    const char* name=desc->GetDisplayName(); offer.name=name?name:"Item";
                    offer.baseValue=desc->GetValue();
                    std::int64_t plain=available;
                    if(desc->extraLists) for(auto* x:*desc->extraLists) if(x) {
                        const auto n=std::max(0,x->GetCount());
                        offer.stacks.push_back({reinterpret_cast<std::uintptr_t>(x),n,signature(offer.object,x)}); plain-=n;
                    }
                    if(plain>0) offer.stacks.push_back({0,static_cast<std::int32_t>(plain),signature(offer.object,nullptr)});
                    std::ranges::sort(offer.stacks,{},&Stack::identity);
                    auto existing=std::ranges::find_if(state->offers,[&](const Offer& row){return sameRow(row,offer);});
                    const auto total=std::int64_t(amount)+(existing!=state->offers.end()?existing->rule.count:0);
                    if(total>offer.rule.available) state->status="That quantity is already in your offer";
                    else if(existing!=state->offers.end() && existing->rule.price!=price) state->status="The price changed. Remove this offer entry and select it again";
                    else if(existing==state->offers.end() && state->offers.size()>=barter::maxLines) state->status=barter::message(barter::Error::tooMany);
                    else {
                        if(existing!=state->offers.end()) existing->rule.count=static_cast<std::int32_t>(total);
                        else {state->offers.push_back(std::move(offer));++state->nextId;}
                        state->status.clear();
                    }
                }
            } else if(operation==Operation::commit) {
                auto lines=rules(*state);
                auto result=barter::evaluate(lines,state->playerGold,state->vendorGold);
                if(result) {state->busy=true;state->status="Exchanging...";commit(state);}
                else state->status=barter::message(result.error);
            }
        }
        reply(p.movie,*state,p.retVal);
    }
};
bool registerScaleform(RE::GFxMovieView* movie,RE::GFxValue* root) {
    const char* url=movie->GetMovieDef()->GetFileURL();
    if(!url || std::string_view(url).find("bartermenu.swf")==std::string_view::npos) return true;
    auto state=std::make_shared<State>(); state->movie=movie;
    {std::scoped_lock lock(activeMutex);active=state;}
    for(auto [name,op]:{std::pair{"Open",Operation::open},{"Queue",Operation::queue},{"Remove",Operation::remove},{"Clear",Operation::clear},{"Commit",Operation::commit},{"Close",Operation::close},{"Status",Operation::status}}) {
        RE::GFxValue function;
        auto handler=RE::make_gptr<Handler>(state,op);
        movie->CreateFunction(&function,handler.get()); root->SetMember(name,function);
    }
    numberMember(*root,"version",1);
    return true;
}
class MenuEvents:public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* event,RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
        if(event && event->menuName==RE::BarterMenu::MENU_NAME && !event->opening) {
            std::shared_ptr<State> state;
            {std::scoped_lock lock(activeMutex);state=active.lock();}
            if(state) {std::scoped_lock lock(state->mutex);state->closed=true;state->offers.clear();}
        }
        return RE::BSEventNotifyControl::kContinue;
    }
} menuEvents;
void message(SKSE::MessagingInterface::Message* m) {
    if(m->type==SKSE::MessagingInterface::kDataLoaded) RE::UI::GetSingleton()->AddEventSink(&menuEvents);
}
}
extern "C" __declspec(dllexport) constinit SKSE::PluginVersionData SKSEPlugin_Version=[] {
    SKSE::PluginVersionData data{}; data.PluginVersion({0,1,0,0}); data.PluginName("BalancedBarter");
    data.AuthorName("Shen Lin custom mods"); data.UsesAddressLibrary(true); data.UsesStructsPost629(true);
    data.CompatibleVersions({REL::Version{1,6,1170,0}}); return data;
}();
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSE::LoadInterface* skse) {
    if(skse->RuntimeVersion()!=REL::Version{1,6,1170,0}) return false;
    SKSE::Init(skse);
    if(auto path=SKSE::log::log_directory()) {
        *path/="BalancedBarter.log";
        auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(),true);
        auto logger=std::make_shared<spdlog::logger>("BalancedBarter",std::move(sink));
        spdlog::set_default_logger(std::move(logger)); spdlog::flush_on(spdlog::level::info);
    }
    SKSE::log::info("Balanced Barter 0.1.0; Skyrim 1.6.1170; SkyUI 6.11");
    return SKSE::GetScaleformInterface()->Register("BalancedBarter",registerScaleform) && SKSE::GetMessagingInterface()->RegisterListener(message);
}
