#pragma once
#include "Batch.h"
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <MinHook.h>

namespace nativebatch {
struct Context {
    batch::Transaction transaction;
    std::optional<RE::ItemCrafted::Event> event;
    std::uint32_t eventCount{};
};
inline thread_local Context* active=nullptr;
using Remove=RE::ObjectRefHandle* (*)(RE::TESObjectREFR*,RE::ObjectRefHandle*,RE::TESBoundObject*,std::int32_t,RE::ITEM_REMOVE_REASON,RE::ExtraDataList*,RE::TESObjectREFR*,const RE::NiPoint3*,const RE::NiPoint3*);
using Add=void (*)(RE::TESObjectREFR*,RE::TESBoundObject*,RE::ExtraDataList*,std::int32_t,RE::TESObjectREFR*);
using UseSkill=void (*)(RE::Actor*,RE::ActorValue,float,RE::TESForm*);
using Experience=void (*)(RE::PlayerCharacter*,RE::ActorValue,float);
inline REL::Relocation<Remove> originalRemove;
inline REL::Relocation<Add> originalAdd;
inline REL::Relocation<UseSkill> originalUseSkill;
inline Experience originalExperience=nullptr;
inline bool ready=false;
inline RE::ObjectRefHandle* remove(RE::TESObjectREFR* self,RE::ObjectRefHandle* result,RE::TESBoundObject* item,std::int32_t n,RE::ITEM_REMOVE_REASON reason,RE::ExtraDataList* extra,RE::TESObjectREFR* to,const RE::NiPoint3* drop,const RE::NiPoint3* rotate) {
    if(active && self==RE::PlayerCharacter::GetSingleton() && item && !to && reason==RE::ITEM_REMOVE_REASON::kRemove)
        n=active->transaction.removal(item->GetFormID(),n);
    batch::Scope<Context> suspend(active,nullptr);
    // Original is the previously installed vtable entry, INCLUDING SCIE.
    return originalRemove(self,result,item,n,reason,extra,to,drop,rotate);
}
inline void add(RE::TESObjectREFR* self,RE::TESBoundObject* item,RE::ExtraDataList* extra,std::int32_t n,RE::TESObjectREFR* from) {
    if(active && self==RE::PlayerCharacter::GetSingleton() && item)
        n=active->transaction.addition(item->GetFormID(),n);
    batch::Scope<Context> suspend(active,nullptr);
    originalAdd(self,item,extra,n,from);
}
inline void useSkill(RE::Actor* self,RE::ActorValue skill,float points,RE::TESForm* source) {
    if(active && self==RE::PlayerCharacter::GetSingleton() && skill==RE::ActorValue::kSmithing)
        points=active->transaction.experience(points);
    // Prevent double scaling if UseSkill internally calls AddSkillExperience.
    batch::Scope<Context> suspend(active,nullptr);
    originalUseSkill(self,skill,points,source);
}
inline void experience(RE::PlayerCharacter* self,RE::ActorValue skill,float points) {
    if(active && self==RE::PlayerCharacter::GetSingleton() && skill==RE::ActorValue::kSmithing)
        points=active->transaction.experience(points);
    batch::Scope<Context> suspend(active,nullptr);
    originalExperience(self,skill,points);
}
class CraftEvents final:public RE::BSTEventSink<RE::ItemCrafted::Event> {
    RE::BSEventNotifyControl ProcessEvent(const RE::ItemCrafted::Event* e,RE::BSTEventSource<RE::ItemCrafted::Event>*) override {
        if(active && e && e->item && e->item->GetFormID()==active->transaction.selection.output) {
            active->event=*e;++active->eventCount;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
inline CraftEvents events;
inline bool install() {
    if(ready)return true;
    auto init=MH_Initialize();if(init!=MH_OK && init!=MH_ERROR_ALREADY_INITIALIZED)return false;
    REL::Relocation<std::uintptr_t> xp{REL::ID(40488)};
    if(MH_CreateHook(reinterpret_cast<void*>(xp.address()),reinterpret_cast<void*>(experience),reinterpret_cast<void**>(&originalExperience))!=MH_OK)return false;
    if(MH_EnableHook(reinterpret_cast<void*>(xp.address()))!=MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(xp.address()));return false;
    }
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_PlayerCharacter[0]};
    originalRemove=table.write_vfunc(0x56,remove);
    originalAdd=table.write_vfunc(0x5A,add);
    originalUseSkill=table.write_vfunc(0xF7,useSkill);
    RE::ItemCrafted::GetEventSource()->AddEventSink(&events);
    ready=true;return true;
}
inline void completeEvents(const Context& c) {
    // This notification has no quantity field. Repeat only the craft event,
    // never the native crafting routine or its UI/sound/consumption sequence.
    // Inventory events already carry the aggregate quantity.
    if(c.event && c.eventCount==1)
        for(std::uint32_t i=1;i<c.transaction.quantity;++i)
            RE::ItemCrafted::GetEventSource()->SendEvent(&*c.event);
}
}
