#pragma once
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
#include <cstring>
#include <optional>

namespace scie {
// SCIE 2.6.0 APIMessaging.h wire layout, independently declared (no private ABI).
constexpr std::uint32_t requestInventory=0x53435249; // 'SCRI'
constexpr std::uint32_t responseInventory=0x53435049; // 'SCPI'
struct Request { std::int32_t stationType=0; };
struct Response { std::int32_t itemCount{},activeStationType=-1; };
static_assert(sizeof(Request)==4 && sizeof(Response)==8);
inline thread_local std::optional<Response> response;
inline bool listening=false;
inline void receive(SKSE::MessagingInterface::Message* m) {
    if(m && m->sender && std::strcmp(m->sender,"CraftingInventoryExtender")==0 &&
       m->type==responseInventory && m->data && m->dataLen==sizeof(Response)) {
        Response copy;std::memcpy(&copy,m->data,sizeof(copy));response=copy;
    }
}
inline void connect() {
    if(!listening)listening=SKSE::GetMessagingInterface()->RegisterListener("CraftingInventoryExtender",receive);
}
enum class Mode { Player,Shared,Unavailable };
inline Mode mode() {
    if(!GetModuleHandleW(L"CraftingInventoryExtender.dll"))return Mode::Player;
    connect();response.reset();Request r;
    const bool sent=listening && SKSE::GetMessagingInterface()->Dispatch(requestInventory,&r,sizeof(r),"CraftingInventoryExtender");
    // Dispatch is synchronous. Never silently fall back to pocket-only counts
    // when SCIE is installed but its session/API cannot be verified.
    return sent && response && response->activeStationType==0 ? Mode::Shared:Mode::Unavailable;
}
inline std::int32_t sharedCount(RE::TESBoundObject* item) {
    auto* player=RE::PlayerCharacter::GetSingleton();
    auto* changes=player ? player->GetInventoryChanges():nullptr;
    if(!changes || !item)return -1;
    // SCIE InventoryHooks.cpp hook 3: the exact crafting count entry point.
    // Call THROUGH the game's patched entry, never SCIE's unhooked original.
    // This includes the player ONCE and honors the active station/source filters.
    static REL::Relocation<std::int32_t (*)(RE::InventoryChanges*,RE::TESBoundObject*,void*)> get{REL::ID(16109)};
    return get(changes,item,nullptr);
}
}
