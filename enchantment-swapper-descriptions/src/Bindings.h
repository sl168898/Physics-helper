#pragma once
#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>

namespace esd {
using FormID = std::uint32_t;
struct Key {
    FormID object{}, owner{};
    std::uint16_t unique{};
    auto operator<=>(const Key&) const = default;
};
struct Binding {
    FormID enchantment{}, source{};
    auto operator<=>(const Binding&) const = default;
};
// Explicit fields, not compiler-dependent struct padding, in the SKSE co-save.
using Wire = std::array<std::uint32_t, 5>;
inline Wire encode(Key key, Binding value) {
    return {key.object, key.owner, key.unique, value.enchantment, value.source};
}
inline bool decode(const Wire& wire, Key& key, Binding& value) {
    if (!wire[0] || !wire[1] || !wire[2] || wire[2] > 65535 || !wire[3] || !wire[4]) return false;
    key = {wire[0], wire[1], static_cast<std::uint16_t>(wire[2])};
    value = {wire[3], wire[4]};
    return true;
}
class Bindings {
public:
    std::map<Key, Binding> entries;
    std::optional<Binding> find(Key key, FormID enchantment) const {
        const auto it = entries.find(key);
        if (it == entries.end() || !enchantment || it->second.enchantment != enchantment) return {};
        return it->second;
    }
    void move(Key oldKey, Key newKey) {
        if (oldKey == newKey) return;
        const auto it = entries.find(oldKey);
        if (it == entries.end()) return;
        const auto value = it->second;
        entries.erase(it);
        if (newKey.object && newKey.owner && newKey.unique) entries.insert_or_assign(newKey, value);
    }
};
// Recover an old swap only if *every* original item with this exact enchantment
// and item type has the same nonempty description. Never guess among artifacts.
class Catalog {
    struct Candidate { FormID source{}; std::string text; bool ambiguous{}; };
    std::map<std::pair<FormID, bool>, Candidate> candidates;
public:
    void clear() { candidates.clear(); }
    void add(FormID enchantment, bool weapon, FormID source, const std::string& text) {
        if (!enchantment || !source) return;
        const auto [it, inserted] = candidates.try_emplace({enchantment, weapon}, Candidate{source, text, text.empty()});
        if (!inserted && (text.empty() || it->second.text != text)) it->second.ambiguous = true;
    }
    FormID find(FormID enchantment, bool weapon) const {
        const auto it = candidates.find({enchantment, weapon});
        return it != candidates.end() && !it->second.ambiguous ? it->second.source : 0;
    }
};
}
