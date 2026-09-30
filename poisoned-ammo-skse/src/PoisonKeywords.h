#pragma once

#include <RE/Skyrim.h>
#include "Core.h"
#include <map>

namespace pa
{
    // Per-operation index, never a cache across loads. Some providers register
    // keywords in TESDataHandler; others (including Dynamic Tooltips) create a
    // form and assign its EditorID without adding it to that array or the
    // EditorID map. Merge the keyword array with the engine's global FormID map.
    class RuntimeKeywords
    {
        bool initialized = false;
        std::map<std::string, RE::BGSKeyword*> names;

        void add(RE::BGSKeyword* keyword)
        {
            if (!keyword) return;
            const auto editorID = keyword->GetFormEditorID();
            if (!editorID || !validKeywordName(editorID)) return;
            auto [it, inserted] = names.emplace(lower(editorID), keyword);
            if (!inserted && it->second != keyword) it->second = nullptr;
        }

        bool initialize(std::string& error)
        {
            if (initialized) return true;
            const auto data = RE::TESDataHandler::GetSingleton();
            if (!data) { error = "keyword registry is unavailable"; return false; }
            for (auto* keyword : data->GetFormArray<RE::BGSKeyword>()) add(keyword);
            {
                const auto [forms, lock] = RE::TESForm::GetAllForms();
                RE::BSReadLockGuard guard(lock.get());
                if (!forms) {
                    names.clear();
                    error = "global form registry is unavailable";
                    return false;
                }
                // No LookupByID/EditorID calls while holding this map's lock.
                for (const auto& entry : *forms) {
                    const auto form = entry.second;
                    if (form && form->Is(RE::FormType::Keyword)) add(form->As<RE::BGSKeyword>());
                }
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
