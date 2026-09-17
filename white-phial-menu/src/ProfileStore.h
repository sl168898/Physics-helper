#pragma once
#include "Profile.h"
#include <filesystem>
#include <fstream>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

namespace phial
{
    inline ProfileResult loadProfile(const std::filesystem::path& path)
    {
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        if (ec) return { {}, "Cannot inspect configuration: " + ec.message() };
        if (!exists) return {};  // Opt-in: no file means no automatic overrides.
        const auto bytes = std::filesystem::file_size(path, ec);
        if (ec || bytes > 16384) return { {}, "Cannot read configuration or file is too large." };
        std::ifstream input(path, std::ios::binary);
        if (!input) return { {}, "Cannot open configuration for reading." };
        std::string contents(static_cast<std::size_t>(bytes), '\0');
        input.read(contents.data(), static_cast<std::streamsize>(bytes));
        if (!input) return { {}, "Cannot read complete configuration." };
        return parseProfile(contents);
    }

    // The old file remains intact if validation, writing or replacement fails.
    // No INI ships in the ZIP, so installing an update does not replace preferences.
    inline std::string saveProfile(const std::filesystem::path& path, const Profile& profile)
    {
        const auto text = serializeProfile(profile);
        if (!text) return "Shared settings contain an invalid value.";
        std::error_code ec;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) return "Cannot create configuration directory: " + ec.message();
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) return "Cannot write configuration temporary file.";
            output.write(text->data(), static_cast<std::streamsize>(text->size()));
            output.close();
            if (!output) {
                std::filesystem::remove(temporary, ec);
                return "Cannot finish writing configuration.";
            }
        }
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
#else
        std::filesystem::rename(temporary, path, ec);
#endif
        if (ec) {
            const auto error = "Cannot replace configuration: " + ec.message();
            std::filesystem::remove(temporary, ec);
            return error;
        }
        return {};
    }
}
