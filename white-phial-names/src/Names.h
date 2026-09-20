#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace phial {
inline constexpr std::uint32_t maxNameBytes = 4096;
struct Name {
    std::uint32_t potion{};
    std::string text;
    void assign(std::uint32_t id, std::string_view chosen, std::string_view original) {
        potion = id;
        // Recording an ordinary batch also clears the previous custom name.
        text = chosen != original && chosen.size() <= maxNameBytes && chosen.find('\0') == chosen.npos ? chosen : "";
    }
    std::string forPotion(std::uint32_t id) const { return id && id == potion ? text : ""; }
    template<class Resolve> bool restore(std::uint32_t oldID, std::string name, Resolve resolve) {
        *this = {};
        if (!oldID || name.size() > maxNameBytes || name.find('\0') != name.npos) return false;
        const auto id = resolve(oldID);
        if (!id) return false;
        potion = id;
        text = std::move(name);
        return true;
    }
};
}
