// Exercise the production launch hook and deferred inventory transaction.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace RE {
    struct TESForm {
        unsigned id{}; virtual ~TESForm()=default;
        unsigned GetFormID() const {return id;}
        template<class T> T* As(){return dynamic_cast<T*>(this);}
    };
    struct TESBoundObject : TESForm {};
    struct TESAmmo : TESBoundObject { bool bolt=true; bool IsBolt(){return bolt;} };
    struct TESObjectWEAP : TESBoundObject {bool crossbow=true;bool IsCrossbow(){return crossbow;}};
    struct BGSPerk : TESForm {};
    struct AlchemyItem : TESForm {};
    struct EnchantmentItem : TESForm {};
    struct NiPoint3 {float x{},y{},z{};};
    template<class T> struct NiPointer {
        T* ptr{}; NiPointer()=default;NiPointer(T* p):ptr(p){}
        explicit operator bool()const{return ptr!=nullptr;}
        T* get()const{return ptr;} T* operator->()const{return ptr;}
    };
    enum class ITEM_REMOVE_REASON {kRemove};
    struct Actor : TESForm {
        bool owns=true,dead=false,blockRemoval=false;
        unsigned removes{},refunds{};
        std::function<void()> onRemove;
        std::map<TESBoundObject*,std::int32_t> stock;
        bool HasPerk(BGSPerk* p){return owns && p;}
        bool IsDead(){return dead;}
        auto GetInventoryCounts(std::function<bool(TESBoundObject&)> f){
            std::map<TESBoundObject*,std::int32_t> out;
            for(auto [item,n]:stock) { if(f(*item))out.emplace(item,n); }
            return out;
        }
        void RemoveItem(TESBoundObject* p,int n,ITEM_REMOVE_REASON,void*,void*){
            ++removes;if(!blockRemoval)stock[p]-=n;if(onRemove)onRemove();
        }
        void AddObjectToContainer(TESBoundObject* p,void*,int n,void*){stock[p]+=n;++refunds;}
    };
    struct PlayerCharacter : Actor {
        inline static PlayerCharacter* instance{};
        static PlayerCharacter* GetSingleton(){return instance;}
    };
    struct Projectile : TESForm {
        struct ProjectileRot {float x{},z{};};
        struct LaunchData {
            Actor* shooter{};TESAmmo* ammoSource{};TESObjectWEAP* weaponSource{};
            TESForm* spell{};AlchemyItem* poison{};EnchantmentItem* enchantItem{};
            NiPoint3 origin{};float angleX{},angleZ{},power=1,scale=1;
            bool useOrigin{},autoAim=true;
            LaunchData(Actor* p,NiPoint3 o,ProjectileRot r,TESAmmo* a,TESObjectWEAP* w):
                shooter(p),ammoSource(a),weaponSource(w),origin(o),angleX(r.x),angleZ(r.z){}
        };
        struct Runtime {float power=1,scale=1,weaponDamage=123;} runtime;
        NiPoint3 pos{};float pitch{},yaw{};
        Runtime& GetProjectileRuntimeData(){return runtime;}
        NiPoint3 GetPosition(){return pos;}
        float GetAngleX(){return pitch;}float GetAngleZ(){return yaw;}
    };
    struct ArrowProjectile : Projectile {
        struct ArrowRuntime {EnchantmentItem* enchantItem{};AlchemyItem* poison{};} arrow;
        ArrowRuntime& GetArrowRuntimeData(){return arrow;}
    };
    struct ProjectileHandle {Projectile* p{};NiPointer<Projectile> get()const{return {p};}};
    struct TESDataHandler {
        BGSPerk* perk{};
        template<class T> T* LookupForm(unsigned id,const char* file){assert(id==0x804 && std::string(file)=="CoatingMechanist.esp");return static_cast<T*>(perk);}
    };
}
namespace SKSE {
    namespace log {template<class...T>void info(const char*,T&&...){} template<class...T>void warn(const char*,T&&...){} template<class...T>void error(const char*,T&&...){} }
    struct Tasks {std::vector<std::function<void()>> jobs;void AddTask(std::function<void()> f){jobs.push_back(f);}};
    inline Tasks tasks;inline bool tasksAvailable=true;
    inline Tasks* GetTaskInterface(){return tasksAvailable?&tasks:nullptr;}
}
#define RELOCATION_ID(a,b) b
namespace REL {template<class T>struct Relocation {T value;T address(){assert(value==44108);return value;}};}
constexpr int MH_OK=0,MH_ERROR_ALREADY_INITIALIZED=1;
inline int createStatus=0,enableStatus=0;inline void* nativeAddress{};inline unsigned hookRemoves{};
int MH_Initialize(){return MH_ERROR_ALREADY_INITIALIZED;}
int MH_CreateHook(void*,void*,void** original){*original=nativeAddress;return createStatus;}
int MH_EnableHook(void*){return enableStatus;}
int MH_RemoveHook(void*){++hookRemoves;return MH_OK;}
#include "TwinShot.h"

RE::PlayerCharacter player;RE::Actor npc;RE::TESAmmo ammo,otherAmmo;RE::TESObjectWEAP weapon;
RE::AlchemyItem poison;RE::EnchantmentItem enchantment;RE::BGSPerk perk;
std::vector<std::unique_ptr<RE::ArrowProjectile>> projectiles;
std::vector<RE::Projectile::LaunchData> launches;
std::uint64_t generation=1;bool enabled=true,fail=false,throws=false;unsigned prepared=0;
RE::ProjectileHandle* native(RE::ProjectileHandle* result,RE::Projectile::LaunchData& d){
    launches.push_back(d);if(throws)throw std::runtime_error("fixture");
    if(fail){result->p=nullptr;return result;}
    auto p=std::make_unique<RE::ArrowProjectile>();p->id=100+projectiles.size();
    p->pos=d.origin;p->pitch=d.angleX;p->yaw=d.angleZ;
    p->runtime.power=d.power;p->runtime.scale=d.scale;p->runtime.weaponDamage=123;
    p->arrow={d.enchantItem,d.poison};result->p=p.get();projectiles.push_back(std::move(p));return result;
}
void reset(int stock=4){
    SKSE::tasks.jobs.clear();projectiles.clear();launches.clear();
    player.stock={{&ammo,stock},{&otherAmmo,100}};player.owns=true;player.dead=false;
    player.blockRemoval=false;player.removes=player.refunds=0;player.onRemove={};
    enabled=true;fail=throws=false;prepared=0;weapon.crossbow=true;ammo.bolt=true;
    SKSE::tasksAvailable=true;twin::installed=true;twin::extraLaunch=false;twin::original=native;twin::perk=&perk;
}
RE::Projectile::LaunchData data(){return {&player,{1,2,3},{.2f,1.f},&ammo,&weapon};}
void flush(){auto tasks=std::move(SKSE::tasks.jobs);SKSE::tasks.jobs.clear();for(auto& f:tasks)f();}
void primary(bool removalBefore=false){
    auto d=data();d.enchantItem=&enchantment;d.power=.8f;d.scale=1.3f;
    if(removalBefore)--player.stock[&ammo];
    RE::ProjectileHandle handle;auto returned=twin::launch(&handle,d);assert(returned==&handle);
    if(!removalBefore && handle.p)--player.stock[&ammo];
}
int main(){
    RE::PlayerCharacter::instance=&player;ammo.id=1;otherAmmo.id=2;weapon.id=3;poison.id=4;
    twin::usable=[] {return enabled;};twin::epoch=[] {return generation;};
    twin::prepare=[](RE::ArrowProjectile* p){++prepared;p->arrow.poison=&poison;};
    // Both possible native ammo-consumption orders: exactly two bolts total.
    for(bool early:{false,true}){
        reset(2);primary(early);assert(launches.size()==1 && SKSE::tasks.jobs.size()==1);
        assert(player.stock[&ammo]==1);projectiles[0]->runtime.weaponDamage=999; // Native caller finalizes damage after Launch returns.
        flush();assert(launches.size()==2 && player.stock[&ammo]==0 && player.removes==1);
        assert(SKSE::tasks.jobs.empty() && !twin::extraLaunch && prepared==2);
        const auto& d=launches[1];assert(d.ammoSource==&ammo && d.weaponSource==&weapon && d.shooter==&player);
        assert(d.poison==&poison && d.enchantItem==&enchantment && d.useOrigin && !d.autoAim);
        assert(d.origin.x==1 && d.origin.y==2 && d.origin.z==3 && d.angleX==.2f);
        assert(std::abs(d.angleZ-(1.f+twin::spreadRadians))<1e-6f);
        assert(d.power==.8f && d.scale==1.3f && projectiles[1]->runtime.weaponDamage==999);
    }
    reset(1);primary();flush();assert(launches.size()==1 && player.stock[&ammo]==0 && player.removes==0);
    // Other coating batches never supply a spare.
    assert(player.stock[&otherAmmo]==100);
    for(int mode=0;mode<7;++mode){
        reset();auto d=data();RE::ProjectileHandle h;
        if(mode==0)player.owns=false;
        if(mode==1)d.shooter=&npc;
        if(mode==2)weapon.crossbow=false;
        if(mode==3)ammo.bolt=false;
        if(mode==4)enabled=false;
        if(mode==5)d.spell=&poison;
        if(mode==6)twin::extraLaunch=true;
        twin::launch(&h,d);assert(launches.size()==1 && SKSE::tasks.jobs.empty());
    }
    reset();fail=true;primary();assert(SKSE::tasks.jobs.empty() && player.stock[&ammo]==4);
    reset();SKSE::tasksAvailable=false;primary();assert(SKSE::tasks.jobs.empty());
    reset();primary();++generation;flush();assert(launches.size()==1 && player.removes==0);
    reset();primary();enabled=false;flush();assert(launches.size()==1 && player.removes==0);
    reset();primary();player.owns=false;flush();assert(launches.size()==1 && player.removes==0);
    reset();primary();player.dead=true;flush();assert(launches.size()==1 && player.removes==0);
    reset();primary();player.blockRemoval=true;flush();assert(launches.size()==1 && player.stock[&ammo]==3);
    for(bool throwing:{false,true}){
        reset();primary();fail=!throwing;throws=throwing;flush();
        assert(player.stock[&ammo]==3 && player.refunds==1 && !twin::extraLaunch && SKSE::tasks.jobs.empty());
    }
    reset();primary();player.onRemove=[] {++generation;};flush();
    assert(launches.size()==1 && player.refunds==0); // Never refund into another save.
    // Two pending discharges compete safely for one remaining matching bolt.
    reset(3);primary();primary();flush();assert(launches.size()==3 && player.stock[&ammo]==0 && player.removes==1);
    // A native shot without coating remains a normal physical Twin Shot.
    reset(2);auto coat=twin::prepare;twin::prepare={};primary();flush();
    assert(launches.size()==2 && !launches[1].poison);twin::prepare=coat;
    // Hook failure disables only this perk, without an invalid original call.
    RE::TESDataHandler forms;forms.perk=&perk;nativeAddress=reinterpret_cast<void*>(&native);
    const auto allowed=twin::usable;const auto clock=twin::epoch;
    reset();twin::installed=false;createStatus=2;twin::install(&forms,false,allowed,clock,coat);assert(!twin::installed);
    createStatus=0;enableStatus=2;twin::install(&forms,false,allowed,clock,coat);assert(!twin::installed && hookRemoves==1);
    enableStatus=0;twin::install(&forms,false,allowed,clock,coat);assert(twin::installed);
    std::cout<<"PASS: production Twin Shot launch, coating snapshot, exact ammo cost, last bolt, failed launch refund, spread, exclusions, nonrecursion and stale-load cancellation\n";
}
