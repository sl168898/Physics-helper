#include "GreenThumb.h"
#include <cassert>
#include <iostream>
#include <limits>
#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#define ABI
#else
#include <sys/mman.h>
#define ABI __attribute__((ms_abi))
#endif
using namespace traits::greenThumb;
using Native = void(ABI *)(std::uint32_t, void*, void*, float*);
static void ABI nativeHarvest(std::uint32_t point, void* owner, void* ingredient, float* value) {
    assert(point == 87 && owner == reinterpret_cast<void*>(1) && ingredient == reinterpret_cast<void*>(2));
    *value = 2.f; // An existing Set Value double-harvest perk.
}
static Native harvestOriginal = nativeHarvest;
static void ABI greenHarvest(std::uint32_t point, void* owner, void* ingredient, float* value) {
    harvestOriginal(point, owner, ingredient, value);
    *value = harvestYield(*value, true, true, true);
}
static double ABI otherEntry(std::uint32_t point, void* owner, double third, double fourth,
    std::uint64_t fifth, std::uint64_t sixth, double seventh, std::uint64_t eighth) {
    assert(point == 30 && owner == reinterpret_cast<void*>(0x1234));
    assert(third == 3.5 && fourth == 4.25 && fifth == 500 && sixth == 600 && seventh == 7.75 && eighth == 800);
    return third + fourth + seventh;
}
int main() {
    for (float existing : {1.f, 2.f, 3.f}) {
        assert(harvestYield(existing, true, true, true) == existing * 2.f);
        assert(harvestYield(existing, false, true, true) == existing);
        assert(harvestYield(existing, true, false, true) == existing);
        assert(harvestYield(existing, true, true, false) == existing);
    }
    assert(harvestYield(0.f, true, true, true) == 0.f);
    assert(std::isnan(harvestYield(std::numeric_limits<float>::quiet_NaN(), true, true, true)));
    // Execute the actual x64 gate using the Microsoft ABI on both platforms.
    // Include integer, XMM and stack arguments on the non-harvest path.
    void* original = reinterpret_cast<void*>(&otherEntry);
    const auto bytes = harvestGate(reinterpret_cast<std::uintptr_t>(&greenHarvest),
        reinterpret_cast<std::uintptr_t>(&original));
#if defined(_WIN32)
    void* memory = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    assert(memory);
#else
    void* memory = mmap(nullptr, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(memory != MAP_FAILED);
#endif
    std::memcpy(memory, bytes.data(), bytes.size());
#if defined(_WIN32)
    FlushInstructionCache(GetCurrentProcess(), memory, bytes.size());
#endif
    float yield = 1.f;
    reinterpret_cast<Native>(memory)(87, reinterpret_cast<void*>(1), reinterpret_cast<void*>(2), &yield);
    assert(yield == 4.f);
    using Other = decltype(&otherEntry);
    assert(reinterpret_cast<Other>(memory)(30, reinterpret_cast<void*>(0x1234), 3.5, 4.25, 500, 600, 7.75, 800) == 15.5);
#if defined(_WIN32)
    VirtualFree(memory, 0, MEM_RELEASE);
#else
    munmap(memory, 4096);
#endif
    std::cout << "Green Thumb: x1/x2/x4 stacking, player/ingredient isolation and native dispatch ABI passed\n";
}
