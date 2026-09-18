#pragma once
#include "Widget.h"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace widget
{
    struct ConfigResult { Settings settings; std::string error; };
    inline std::string_view trim(std::string_view s)
    {
        const auto first = s.find_first_not_of(" \t\r\n");
        return first == s.npos ? std::string_view{} : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
    }
    inline ConfigResult parseConfig(std::string_view source)
    {
        Settings out;
        std::set<std::string> seen;
        while (!source.empty()) {
            auto end = source.find('\n');
            auto line = trim(source.substr(0, end));
            source = end == source.npos ? std::string_view{} : source.substr(end + 1);
            if (line.empty() || line.front() == ';' || line.front() == '#' || line == "[Widget]") continue;
            const auto equal = line.find('=');
            if (equal == line.npos) return { {}, "Invalid configuration line." };
            auto key = trim(line.substr(0, equal));
            auto value = trim(line.substr(equal + 1));
            bool* flag = key == "Enabled" ? &out.enabled : key == "ShowLabel" ? &out.showLabel :
                key == "HideWhenAbsent" ? &out.hideAbsent : key == "HideInMenus" ? &out.hideInMenus : nullptr;
            float* number = key == "X" ? &out.x : key == "Y" ? &out.y : key == "Scale" ? &out.scale :
                key == "Opacity" ? &out.opacity : nullptr;
            if (!flag && !number) continue; // Allow settings added by later versions.
            if (!seen.emplace(key).second) return { {}, "Duplicate setting: " + std::string(key) };
            if (flag) {
                if (value != "0" && value != "1") return { {}, "Expected 0 or 1 for " + std::string(key) };
                *flag = value == "1";
            } else {
                auto result = std::from_chars(value.data(), value.data() + value.size(), *number);
                if (result.ec != std::errc{} || result.ptr != value.data() + value.size())
                    return { {}, "Invalid number for " + std::string(key) };
            }
        }
        return valid(out) ? ConfigResult{ out, {} } : ConfigResult{ {}, "Settings are outside their allowed ranges." };
    }
    inline std::string serializeConfig(const Settings& value)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::setprecision(9) << "; White Phial Widget - shared across saves in this MO2 profile\n[Widget]\n"
            << "Enabled=" << value.enabled << "\nShowLabel=" << value.showLabel
            << "\nHideWhenAbsent=" << value.hideAbsent << "\nHideInMenus=" << value.hideInMenus
            << "\nX=" << value.x << "\nY=" << value.y << "\nScale=" << value.scale << "\nOpacity=" << value.opacity << '\n';
        return out.str();
    }
    inline ConfigResult loadConfig(const std::filesystem::path& path)
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(path, ec);
        if (ec) return { {}, "Cannot inspect widget settings: " + ec.message() };
        if (!exists) return {};
        auto size = std::filesystem::file_size(path, ec);
        if (ec || size > 16384) return { {}, "Cannot read widget settings or file is too large." };
        std::ifstream file(path, std::ios::binary);
        std::string source(static_cast<std::size_t>(size), '\0');
        file.read(source.data(), static_cast<std::streamsize>(size));
        if (!file) return { {}, "Cannot read widget settings." };
        return parseConfig(source);
    }
    inline std::string saveConfig(const std::filesystem::path& path, const Settings& value)
    {
        if (!valid(value)) return "Widget settings are invalid.";
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return "Cannot create settings directory: " + ec.message();
        auto temp = path; temp += ".tmp";
        {
            std::ofstream file(temp, std::ios::binary | std::ios::trunc);
            file << serializeConfig(value);
            file.close();
            if (!file) { std::filesystem::remove(temp, ec); return "Cannot write widget settings."; }
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
        std::filesystem::rename(temp, path, ec);
#endif
        if (ec) {
            auto message = "Cannot replace widget settings: " + ec.message();
            std::filesystem::remove(temp, ec);
            return message;
        }
        return {};
    }
}
