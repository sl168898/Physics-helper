#include "CriticalEntryGate.h"
#include <cassert>
#include <cstdarg>
#include <Windows.h>

unsigned originalCalls{}, criticalCalls{};
int owner{}, weapon{}, target{}, extra{};
float criticalValue{};
void critical(std::uint32_t entry, void* a, void* b, void* c, float* value)
{
    assert((entry == 1 || entry == 2) && a == &owner && b == &weapon && c == &target);
    assert(value == &criticalValue);
    ++criticalCalls; *value += 25;
}
// Deliberately exercise register floats, pointers and stack arguments beyond
// the typed critical thunk. A fixed-signature forwarding hook corrupts this.
std::uint64_t original(std::uint32_t entry, ...)
{
    assert(entry != 1 && entry != 2);
    va_list args; va_start(args, entry);
    assert(va_arg(args, double) == 1.25);
    assert(va_arg(args, void*) == &owner);
    assert(va_arg(args, double) == 9.5);
    assert(va_arg(args, void*) == &weapon);
    assert(va_arg(args, std::uint64_t) == 0xFEDCBA9876543210ull);
    assert(va_arg(args, double) == -7.125);
    assert(va_arg(args, void*) == &extra);
    va_end(args);
    ++originalCalls;
    return 0x123456789ABCDEF0ull;
}
int main()
{
    const auto code = precision::criticalEntryGate(reinterpret_cast<std::uintptr_t>(&critical),
        reinterpret_cast<std::uintptr_t>(&original));
    void* memory = VirtualAlloc(nullptr, code.size(), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    assert(memory); std::memcpy(memory, code.data(), code.size());
    DWORD previous{};
    assert(VirtualProtect(memory, code.size(), PAGE_EXECUTE_READ, &previous));
    assert(FlushInstructionCache(GetCurrentProcess(), memory, code.size()));
    using Critical = decltype(&critical);
    for (std::uint32_t entry : {1u, 2u})
        reinterpret_cast<Critical>(memory)(entry, &owner, &weapon, &target, &criticalValue);
    assert(criticalCalls == 2 && originalCalls == 0 && criticalValue == 50);
    using Variadic = decltype(&original);
    for (std::uint32_t entry : {0u, 3u, 17u, 72u, 255u, 0xFFFFFFFFu}) {
        auto result = reinterpret_cast<Variadic>(memory)(entry, 1.25, static_cast<void*>(&owner), 9.5,
            static_cast<void*>(&weapon), 0xFEDCBA9876543210ull, -7.125, static_cast<void*>(&extra));
        assert(result == 0x123456789ABCDEF0ull);
    }
    assert(originalCalls == 6 && criticalCalls == 2);
    VirtualFree(memory, 0, MEM_RELEASE);
}
