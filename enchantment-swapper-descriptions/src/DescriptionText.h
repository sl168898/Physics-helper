#pragma once
#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace esd {
inline std::string lowerASCII(std::string_view text) {
    std::string result(text);
    for (auto& ch : result) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    return result;
}
inline std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}
inline bool numericMarker(std::string_view text) {
    text = trim(text);
    if (!text.empty() && (text.front() == '+' || text.front() == '-')) text.remove_prefix(1);
    bool digit = false;
    for (const auto ch : text) {
        if (ch >= '0' && ch <= '9') digit = true;
        else if (ch != '.' && ch != ',' && ch != ' ' && ch != '%') return false;
    }
    return digit;
}
inline bool htmlTag(std::string_view token) {
    if (token.starts_with('/')) token.remove_prefix(1);
    const auto end = token.find_first_of(" \t\r\n/");
    token = token.substr(0, end);
    return token == "font" || token == "b" || token == "i" || token == "u" ||
        token == "br" || token == "p" || token == "a" || token == "span" || token == "textformat";
}
// WEAP/ARMO DESC uses <15>/<60> to mark numbers. These are not HTML tags.
// Resolve those markers before Scaleform's htmlText parser can discard them.
// Global substitutions are read at display time, not frozen in the save.
template<class ResolveGlobal>
std::optional<std::string> formatDescription(std::string_view raw, bool plain, ResolveGlobal resolveGlobal) {
    std::string result;
    result.reserve(raw.size());
    for (std::size_t pos = 0; pos < raw.size();) {
        if (raw[pos] != '<') { result.push_back(raw[pos++]); continue; }
        const auto end = raw.find('>', pos + 1);
        if (end == std::string_view::npos) { result.append(raw.substr(pos)); break; }
        const auto inside = trim(raw.substr(pos + 1, end - pos - 1));
        const auto normalized = lowerASCII(inside);
        if (numericMarker(inside)) result.append(inside);
        else if (normalized.starts_with("global=")) {
            const auto value = resolveGlobal(trim(inside.substr(7)));
            if (!value) return std::nullopt; // No fabricated number if the source global is unavailable.
            result.append(*value);
        } else if (normalized == "mag" || normalized == "dur" || normalized == "area") {
            return std::nullopt; // These need a magic-effect context; retain the engine's original card.
        } else if (plain && htmlTag(normalized)) {
            if (normalized == "br" || normalized == "br/" || normalized == "br /" || normalized == "/p") result += '\n';
        } else result.append(raw.substr(pos, end - pos + 1));
        pos = end + 1;
    }
    if (plain) {
        for (const auto& [entity, character] : {
                 std::pair<std::string_view, std::string_view>{"&lt;", "<"}, {"&gt;", ">"},
                 {"&quot;", "\""}, {"&apos;", "'"}, {"&nbsp;", " "}, {"&amp;", "&"}}) {
            std::size_t pos = 0;
            while ((pos = result.find(entity, pos)) != std::string::npos) {
                result.replace(pos, entity.size(), character);
                pos += character.size();
            }
        }
    }
    return result;
}
}
