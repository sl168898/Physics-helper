#pragma once
// Binary bridge for William_Rod's MIT-licensed 1.1.0 release only.
// See BRIDGE-NOTES.md. No Skyrim executable offsets are used here.
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace sr::bridge {
inline constexpr char expectedHash[] = "36716c8750e1acad6d48bda14e127c9ebc2a24fcf002049f39f6d8551339cbc8";
inline constexpr std::uintptr_t preflightRva=0x33700, inputSlotRva=0x949e8, inputFunctionRva=0x2ac00;
inline constexpr std::uintptr_t kernelRva=0x51060, rechargeKeyRva=0xb744c, snapshotKeyRva=0xb7450;
inline constexpr std::array<std::uint8_t,6> expectedPreflight{0x48,0x8b,0x01,0xff,0x60,0x08};
inline std::atomic_bool automatic{false};
inline std::atomic_uint64_t blocked{0};
inline std::uintptr_t base{}, originalInput{};
inline void* nearby{};
inline bool gateInstalled{}, inputInstalled{};

inline std::string sha256(const std::filesystem::path& file) {
    std::ifstream in(file,std::ios::binary);
    if(!in) return {};
    BCRYPT_ALG_HANDLE alg{}; BCRYPT_HASH_HANDLE hash{};
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0) return {};
    DWORD size{}, returned{};
    bool ok=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&returned,0)>=0;
    std::vector<UCHAR> object(size);
    if(ok) ok=BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0;
    std::array<char,65536> buffer{};
    while(ok && in) {
        in.read(buffer.data(),buffer.size());
        if(in.gcount()) ok=BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),static_cast<ULONG>(in.gcount()),0)>=0;
    }
    std::array<UCHAR,32> digest{};
    if(ok && !in.bad()) ok=BCryptFinishHash(hash,digest.data(),digest.size(),0)>=0; else ok=false;
    if(hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg,0);
    if(!ok) return {};
    std::string out; constexpr char digits[]="0123456789abcdef";
    for(auto b:digest) {out+=digits[b>>4];out+=digits[b&15];}
    return out;
}
inline bool writeMemory(std::uintptr_t where,const void* data,std::size_t size) {
    DWORD old{};
    if(!VirtualProtect(reinterpret_cast<void*>(where),size,PAGE_EXECUTE_READWRITE,&old)) return false;
    std::memcpy(reinterpret_cast<void*>(where),data,size);
    FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<void*>(where),size);
    DWORD ignored{};
    VirtualProtect(reinterpret_cast<void*>(where),size,old,&ignored);
    return true;
}
inline bool preflight(void* context,void* receipt) {
    auto* bytes=static_cast<std::byte*>(receipt);
    // The original manual dispatcher marks the receipt before entering the kernel.
    if(!automatic.load(std::memory_order_acquire) && bytes[0x288]==std::byte{0}) {
        using Assign=void*(*)(void*,const char*,std::size_t);
        // Use the original string implementation/allocator for its receipt.
        reinterpret_cast<Assign>(base+0x21830)(bytes+0x298,"MANUAL_MODE",11);
        blocked.fetch_add(1,std::memory_order_relaxed);
        return false;
    }
    // Exact semantics of the displaced six-byte virtual dispatch.
    using Stage=bool(*)(void*,void*);
    auto* vtable=*reinterpret_cast<std::uintptr_t**>(context);
    return reinterpret_cast<Stage>(vtable[1])(context,receipt);
}
inline bool install(HMODULE module,std::string& error) {
    if(gateInstalled) return true;
    if(!module) {error="Soul Gem Auto Recharge 1.1.0 is not loaded.";return false;}
    std::array<wchar_t,32768> path{};
    const auto n=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if(!n || n==path.size() || sha256(path.data())!=expectedHash) {
        error="Unsupported original DLL. Install the supplied Soul Gem Auto Recharge 1.1.0 files.";return false;
    }
    base=reinterpret_cast<std::uintptr_t>(module);
    originalInput=*reinterpret_cast<std::uintptr_t*>(base+inputSlotRva);
    if(std::memcmp(reinterpret_cast<void*>(base+preflightRva),expectedPreflight.data(),expectedPreflight.size()) || originalInput!=base+inputFunctionRva) {
        error="Another patch has changed the recharge bridge. No hooks were installed.";base=0;return false;
    }
    SYSTEM_INFO info{};GetSystemInfo(&info);
    const auto stride=static_cast<std::uintptr_t>(info.dwAllocationGranularity);
    const auto origin=(base+preflightRva)&~(stride-1);
    for(std::uintptr_t delta=stride;!nearby && delta<0x70000000;delta+=stride) {
        for(auto address:{origin+delta,origin-delta}) {
            MEMORY_BASIC_INFORMATION region{};
            if(VirtualQuery(reinterpret_cast<void*>(address),&region,sizeof(region)) && region.State==MEM_FREE)
                nearby=VirtualAlloc(reinterpret_cast<void*>(address),4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
            if(nearby) break;
        }
    }
    if(!nearby) {error="Could not allocate the recharge bridge.";base=0;return false;}
    const auto target=reinterpret_cast<std::uintptr_t>(&preflight);
    std::memcpy(nearby,&target,sizeof(target));
    DWORD old{};VirtualProtect(nearby,4096,PAGE_READONLY,&old);
    std::array<std::uint8_t,6> jump{0xff,0x25,0,0,0,0};
    const auto displacement=static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(nearby)-static_cast<std::intptr_t>(base+preflightRva+6));
    std::memcpy(jump.data()+2,&displacement,sizeof(displacement));
    if(!writeMemory(base+preflightRva,jump.data(),jump.size())) {
        VirtualFree(nearby,0,MEM_RELEASE);nearby=nullptr;base=0;error="Could not install the recharge bridge.";return false;
    }
    gateInstalled=true;return true;
}
inline bool installInput(std::uintptr_t replacement) {
    if(!gateInstalled || inputInstalled) return false;
    inputInstalled=writeMemory(base+inputSlotRva,&replacement,sizeof(replacement));return inputInstalled;
}
inline void uninstallForTest() {
    if(inputInstalled) writeMemory(base+inputSlotRva,&originalInput,sizeof(originalInput));
    if(gateInstalled) writeMemory(base+preflightRva,expectedPreflight.data(),expectedPreflight.size());
    if(nearby) VirtualFree(nearby,0,MEM_RELEASE);
    nearby=nullptr;gateInstalled=false;inputInstalled=false;base=0;
}
inline std::uint32_t rechargeKey() {return *reinterpret_cast<const std::uint32_t*>(base+rechargeKeyRva);}
inline std::uint32_t snapshotKey() {return *reinterpret_cast<const std::uint32_t*>(base+snapshotKeyRva);}
}
