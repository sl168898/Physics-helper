// Executes the unmodified original recharge kernel in a Windows process with
// fake stage callbacks. No Skyrim engine functions or player inventory are used.
#include "CoreBridge.h"
#include "InputRules.h"
#include <cassert>
#include <iostream>
#include <new>

struct Receipt {
    alignas(8) std::array<std::byte,0x298> bytes{};
    std::string reason;
};
static_assert(offsetof(Receipt,reason)==0x298 && sizeof(Receipt)==0x2b8);
struct Context {
    std::uintptr_t* vtable;
    unsigned calls{},consume{},failAt{99};
};
bool stage(void* raw,void*) {
    auto& context=*static_cast<Context*>(raw);
    const auto index=context.calls++;
    if(index==context.failAt) return false;
    if(index==2) ++context.consume;
    return true;
}
void runKernel(bool automatic,bool manual,bool dry,unsigned fail=99) {
    sr::bridge::automatic=automatic;
    std::uintptr_t vt[7]{};for(unsigned i=1;i<7;++i) vt[i]=reinterpret_cast<std::uintptr_t>(&stage);
    Context ctx{vt,0,0,fail};Receipt receipt;
    receipt.bytes[0x288]=manual?std::byte{1}:std::byte{0};
    std::array<std::byte,2> engine{};
    using Kernel=void(*)(void*,void*,void*,bool);
    reinterpret_cast<Kernel>(sr::bridge::base+sr::bridge::kernelRva)(engine.data(),&ctx,&receipt,dry);
    const bool blocked=!automatic && !manual;
    if(blocked) {
        assert(ctx.calls==0 && ctx.consume==0 && receipt.reason=="MANUAL_MODE");
        assert(engine[0]==std::byte{0} && engine[1]==std::byte{0});
        assert(receipt.bytes[0x284]==std::byte{0});
    } else if(fail==0) {
        assert(ctx.calls==1 && ctx.consume==0);
        assert(engine[0]==std::byte{0} && engine[1]==std::byte{0});
    } else if(dry) {
        assert(ctx.calls==2 && ctx.consume==0 && receipt.reason=="DRY_RUN_PLANNED");
    } else {
        assert(ctx.calls==6 && ctx.consume==1 && receipt.reason=="VERIFIED");
        assert(engine[0]==std::byte{0} && engine[1]==std::byte{0});
    }
}
int wmain(int argc,wchar_t** argv) {
    assert(argc==2);
    auto path=std::filesystem::absolute(argv[1]);
    assert(sr::bridge::sha256(path)==sr::bridge::expectedHash);
    HMODULE core=LoadLibraryW(path.c_str());
    if(!core) {std::cerr<<"LoadLibrary failed: "<<GetLastError()<<'\n';return 1;}
    std::string error;
    assert(!sr::bridge::install(GetModuleHandleW(nullptr),error));
    assert(!sr::bridge::gateInstalled);
    assert(sr::bridge::install(core,error));
    unsigned scenarios{};
    for(int repetition=0;repetition<20;++repetition)
        for(bool automatic:{false,true}) for(bool manual:{false,true}) for(bool dry:{false,true}) for(unsigned fail:{0u,99u}) {
            runKernel(automatic,manual,dry,fail);++scenarios;
        }
    // Switching modes between requests must not retain the engine's error latch.
    runKernel(false,false,false);runKernel(false,true,false);runKernel(true,false,false);
    assert(sr::bridge::blocked==81);
    for(unsigned key=0;key<300;++key) {
        for(bool down:{false,true}) for(bool ready:{false,true}) for(bool blocked:{false,true}) for(bool capture:{false,true}) {
            const auto result=sr::route(key,88,68,down,ready,blocked,capture);
            const auto expected=(!down || !ready || blocked || capture) ? sr::InputAction::ignore : key==88 ? sr::InputAction::recharge : key==68 ? sr::InputAction::snapshot : sr::InputAction::ignore;
            assert(result==expected);++scenarios;
        }
    }
    assert(sr::bindable(87) && sr::bindable(88) && !sr::bindable(1) && !sr::bindable(41) && !sr::bindable(256));
    assert(sr::route(87,88,68,true,true,false,false)==sr::InputAction::ignore);
    sr::bridge::uninstallForTest();
    assert(std::memcmp(reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(core)+sr::bridge::preflightRva),sr::bridge::expectedPreflight.data(),6)==0);
    FreeLibrary(core);
    std::cout<<scenarios+3<<" real-kernel and input scenarios passed. No in-game test performed.\n";
}
