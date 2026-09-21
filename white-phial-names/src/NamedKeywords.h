#pragma once
#include "Bank.h"
#include <map>
#include <string_view>

namespace phial::storage {
// KID adds its generated keywords to TESDataHandler's keyword array, not
// necessarily to the engine's general EditorID map. Build this index only
// during protection/load, and never serialize the process-specific pointers.
template<class T> class NamedKeywords {
    std::map<std::string, T*> entries;
    static std::string key(std::string_view name) {
        std::string result(name);
        for (auto& c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        return result;
    }
public:
    void add(std::string_view name, T* keyword) {
        if (!keyword || name.empty()) return;
        auto [it, inserted] = entries.emplace(key(name), keyword);
        if (!inserted && it->second != keyword) it->second = nullptr;
    }
    T* resolve(const std::string& name) const {
        if (name.empty()) throw Error("Runtime keyword has no EditorID");
        const auto it = entries.find(key(name));
        if (it == entries.end()) throw Error("Missing named keyword: " + name);
        if (!it->second) throw Error("Ambiguous named keyword: " + name);
        return it->second;
    }
};
}
