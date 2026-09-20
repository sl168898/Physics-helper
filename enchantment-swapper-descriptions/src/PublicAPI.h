#pragma once
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>

namespace esd::api {
// C ABI: no STL objects or borrowed inventory pointers cross the DLL boundary.
// Returns required bytes INCLUDING the NUL, or 0 when no custom transfer text
// applies. A null/short buffer does not receive a truncated description.
using GetDescriptionV1 = std::uint32_t (*)(std::uint32_t objectID,
    std::uint32_t enchantmentID, std::uint32_t ownerID, std::uint16_t uniqueID,
    char* buffer, std::uint32_t capacity);
inline constexpr auto exportName = "ESD_GetDescriptionV1";
inline std::uint32_t copyText(std::string_view text, char* buffer, std::uint32_t capacity) {
    if (buffer && capacity) buffer[0] = '\0';
    if (text.empty() || text.size() >= std::numeric_limits<std::uint32_t>::max()) return 0;
    const auto required = static_cast<std::uint32_t>(text.size() + 1);
    if (buffer && capacity >= required) {
        std::memcpy(buffer, text.data(), text.size());
        buffer[text.size()] = '\0';
    }
    return required;
}
}
