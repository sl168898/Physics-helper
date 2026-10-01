#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace traits::greenThumb {
inline float harvestYield(float existingYield, bool selected, bool player, bool ingredient) {
    return selected && player && ingredient && std::isfinite(existingYield) && existingYield > 0.f
        ? existingYield * 2.f : existingYield;
}

// Windows x64 ABI gate: only entry point 87 has its arguments interpreted.
// Every other entry point tail-jumps to MinHook's original trampoline with
// argument registers, XMM registers and stack arguments completely untouched.
// RAX and flags are volatile at a function boundary. No generic varargs bridge.
inline std::array<std::uint8_t, 29> harvestGate(std::uintptr_t handler, std::uintptr_t originalPointer) {
    std::array<std::uint8_t, 29> code{
        0x83, 0xF9, 0x57,             // cmp ecx, 87
        0x75, 0x0C,                   // jne passthrough
        0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, handler
        0xFF, 0xE0,                   // jmp rax
        0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, &original
        0xFF, 0x20                    // jmp qword ptr [rax]
    };
    static_assert(sizeof(std::uintptr_t) == 8);
    std::memcpy(code.data() + 7, &handler, 8);
    std::memcpy(code.data() + 19, &originalPointer, 8);
    return code;
}
}
