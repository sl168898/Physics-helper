#pragma once

#include <RE/Skyrim.h>
#include "Core.h"
#include <map>

namespace pa
{
    // Per-operation index, never a cache across loads. Providers such as KID
    // register generated keywords in TESDataHandler's keyword array, which
    // need not be the same as the engine's general EditorID lookup map.
    class RuntimeKeywords
    {
        bool initialized = false;
        std::map<std::string, RE::BGSKeyword*> names;

        bool initialize(std::string& error)
        {
            if (initialized) return true;
            const auto data = RE::TESDataHandler::GetSingleton();
            if (!data) { error = "keyword registry is unavailable"; return false; }
            for (auto* keyword : data->GetFormArray<RE::BGSKeyword>()) {
                if (!keyword) continue;
                const auto editorID = keyword->GetFormEditorID();
                if (!editorID || !validKeywordName(editorID)) continue;
                auto [it, inserted] = names.emplace(lower(editorID), keyword);
                if (!inserted && it->second != keyword) it->second = nullptr;
            }
            initialized = true;
            return true;
        }

    public:
        RE::BGSKeyword* resolve(const std::string& editorID, std::string& error)
        {
            error.clear();
            if (!validKeywordName(editorID)) { error = "keyword has no valid stable EditorID"; return nullptr; }
            if (!initialize(error)) return nullptr;
            const auto found = names.find(lower(editorID));
            if (found == names.end()) { error = "named keyword is not registered"; return nullptr; }
            if (!found->second) { error = "keyword EditorID is ambiguous"; return nullptr; }
            return found->second;
        }

        std::optional<std::string> capture(RE::BGSKeyword* keyword, std::string& error)
        {
            error.clear();
            if (!keyword || !keyword->IsDynamicForm()) {
                error = "keyword has no stable source record"; return {};
            }
            const auto editorID = keyword->GetFormEditorID();
            if (!editorID || !validKeywordName(editorID)) {
                error = "runtime keyword has no valid stable EditorID"; return {};
            }
            const auto resolved = resolve(editorID, error);
            if (!resolved) return {};
            if (resolved != keyword) { error = "keyword EditorID resolves to a different object"; return {}; }
            return lower(editorID);
        }
    };
}
