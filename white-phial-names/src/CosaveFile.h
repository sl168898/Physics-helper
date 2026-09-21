#pragma once
#include "Bank.h"
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>

namespace phial::storage {
inline std::string cosaveFilename(std::string_view saveName) {
    // SKSE 2.2.6 SetSaveName removes a terminal .ess case-insensitively.
    // PreLoadGame carries the original name, not that normalized name.
    while (!saveName.empty() && saveName.back() == '\0') saveName.remove_suffix(1);
    if (saveName.find('\0') != std::string_view::npos) throw Error("Invalid save filename");
    if (saveName.size() >= 4) {
        auto suffix = saveName.substr(saveName.size() - 4);
        if (suffix[0] == '.' && (suffix[1] == 'e' || suffix[1] == 'E') &&
            (suffix[2] == 's' || suffix[2] == 'S') && (suffix[3] == 's' || suffix[3] == 'S'))
            saveName.remove_suffix(4);
    }
    if (saveName.empty()) throw Error("Empty save filename");
    return std::string(saveName) + ".skse";
}
inline std::filesystem::path cosavePath(const std::filesystem::path& documents,
    const std::filesystem::path& localSavePath, std::string_view saveName) {
    // Match SKSE 2.2.6 MakeSavePath for our supported Steam 1.6.1170 runtime.
    // Append the INI path and save name as SKSE does; filesystem operator/=
    // could unexpectedly replace the root for a rooted component. Relative
    // profile paths remain intact. Never search other folders or saves.
    auto path = documents / "My Games" / "Skyrim Special Edition";
    path += std::filesystem::path::preferred_separator;
    path += localSavePath;
    path += std::filesystem::path::preferred_separator;
    path += cosaveFilename(saveName);
    return path;
}
inline std::optional<Bytes> readCosaveFile(const std::filesystem::path& path) {
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) throw Error("Cannot check SKSE co-save: " + ec.message());
    if (!exists) return {}; // An older save may legitimately have no co-save.
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) throw Error("Cannot open existing SKSE co-save");
    const auto size = in.tellg();
    if (size < 0 || size > 256 * 1024 * 1024) throw Error("Unsupported SKSE co-save size");
    Bytes bytes(static_cast<std::size_t>(size)); in.seekg(0);
    if (!bytes.empty() && !in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
        throw Error("Cannot read SKSE co-save");
    return bytes;
}
}
