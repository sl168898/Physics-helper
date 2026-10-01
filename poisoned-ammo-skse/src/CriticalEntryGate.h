#pragma once
#include <array>
#include <cstdint>
#include <cstring>

namespace precision
{
    // Windows x64 tail gate. ONLY critical chance/damage have our known five
    // argument signature. All other variadic entry points must preserve every
    // register and stack argument, including arguments beyond the fifth.
    inline std::array<std::uint8_t, 34> criticalEntryGate(std::uintptr_t critical, std::uintptr_t original)
    {
        static_assert(sizeof(std::uintptr_t) == 8);
        std::array<std::uint8_t, 34> code{
            0x83, 0xF9, 0x01,       // cmp ecx, 1
            0x74, 0x05,             // je critical
            0x83, 0xF9, 0x02,       // cmp ecx, 2
            0x75, 0x0C,             // jne original
            0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, critical
            0xFF, 0xE0,             // jmp rax
            0x48, 0xB8, 0,0,0,0,0,0,0,0, // mov rax, original
            0xFF, 0xE0              // jmp rax
        };
        std::memcpy(code.data() + 12, &critical, 8);
        std::memcpy(code.data() + 24, &original, 8);
        return code;
    }
}
