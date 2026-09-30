#pragma once

#include <RE/Skyrim.h>
#include "Core.h"
#include "PoisonKeywords.h"

namespace pa::crafted
{
    enum class Reason { none, notPoison, noSource, scriptInspection, customScript,
        effectCount, missingEffect, effectConditions, effectSource, keywordSource, keywordIdentity };
    struct Issue
    {
        Reason reason = Reason::none;
        RE::FormID related{};
        std::size_t index{};
        std::string detail;
    };
    inline const char* description(Reason reason)
    {
        switch (reason) {
        case Reason::notPoison: return "selected item is not a poison";
        case Reason::noSource: return "this poison has no stable source record";
        case Reason::scriptInspection: return "could not inspect this poison's attached scripts";
        case Reason::customScript: return "this crafted poison has an unsupported attached script";
        case Reason::effectCount: return "this poison has an unsupported number of effects";
        case Reason::missingEffect: return "this poison has an invalid magic effect";
        case Reason::effectConditions: return "this crafted poison has unsupported effect conditions";
        case Reason::effectSource: return "this poison uses a temporary or unresolved magic effect";
        case Reason::keywordSource: return "this poison uses a temporary or unresolved keyword";
        case Reason::keywordIdentity: return "a runtime keyword cannot be identified safely; see PoisonedAmmoNative.log";
        default: return "unsupported poison data";
        }
    }

    inline Issue inspectScripts(RE::AlchemyItem* item)
    {
        const auto vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
        const auto policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
        if (!policy) return { Reason::scriptInspection, 0, 0, "VM/handle policy unavailable" };
        const auto handle = policy->GetHandleForObject(item->GetFormType(), item);
        if (handle == policy->EmptyHandle()) return {};

        // The pinned TESForm::HasVMAD only tests for a nonempty VM handle.
        // A handle is NOT evidence of attached custom scripts. Inspect actual
        // bound objects while holding the VM's lock; retain no object pointers.
        RE::BSSpinLockGuard lock(vm->attachedScriptsLock);
        const auto found = vm->attachedScripts.find(handle);
        if (found == vm->attachedScripts.end()) return {};
        for (const auto& attached : found->second) {
            if (!attached) continue;
            const auto type = attached->GetTypeInfo();
            const auto name = type ? type->GetName() : nullptr;
            if (!name || !*name) return { Reason::scriptInspection, 0, 0, "Bound object has no script type" };
            const auto normalized = pa::lower(name);
            // Papyrus' native Potion/Form wrappers carry no custom item script
            // state. A subclass is still rejected even if its parent is Potion.
            if (normalized != "potion" && normalized != "form")
                return { Reason::customScript, 0, 0, name };
        }
        return {};
    }

    template<class KeyOf, class NameOf>
    std::optional<pa::Poison> capture(RE::AlchemyItem* item, KeyOf keyOf, NameOf nameOf, Issue* issue = nullptr)
    {
        if (issue) *issue = {};
        const auto reject = [&](Reason reason, const RE::TESForm* related = nullptr,
                                std::size_t index = 0, std::string detail = {}) -> std::optional<pa::Poison> {
            if (issue) *issue = { reason, related ? related->GetFormID() : 0, index, std::move(detail) };
            return {};
        };
        if (!item || !item->IsPoison()) return reject(Reason::notPoison);
        pa::Poison result; result.name = nameOf(item);
        // Static poison records keep their original effect conditions/scripts.
        if (auto key = keyOf(item)) { result.source = *key; return result; }
        if (!item->IsDynamicForm()) return reject(Reason::noSource, item);
        const auto scripts = inspectScripts(item);
        if (scripts.reason != Reason::none) {
            if (issue) *issue = scripts;
            return {};
        }
        if (item->effects.empty() || item->effects.size() > 64) return reject(Reason::effectCount);
        result.value = item->data.costOverride; result.flags = item->data.flags.underlying();
        std::size_t index = 0;
        for (const auto effect : item->effects) {
            if (!effect || !effect->baseEffect) return reject(Reason::missingEffect, nullptr, index);
            if (effect->conditions.head) return reject(Reason::effectConditions, effect->baseEffect, index);
            const auto key = keyOf(effect->baseEffect);
            if (!key) return reject(Reason::effectSource, effect->baseEffect, index);
            result.effects.push_back({ *key, effect->effectItem.magnitude, effect->effectItem.area,
                effect->effectItem.duration, effect->cost });
            ++index;
        }
        index = 0;
        pa::RuntimeKeywords runtimeKeywords;
        for (const auto keyword : item->GetKeywords()) {
            const auto key = keyOf(keyword);
            if (key) result.keywords.push_back(*key);
            else if (keyword && keyword->IsDynamicForm()) {
                std::string error;
                const auto name = runtimeKeywords.capture(keyword, error);
                if (!name) {
                    const auto editorID = keyword->GetFormEditorID();
                    return reject(Reason::keywordIdentity, keyword, index,
                        "EditorID='" + std::string(editorID ? editorID : "") + "': " + error);
                }
                result.namedKeywords.push_back(*name);
            } else return reject(Reason::keywordSource, keyword, index);
            ++index;
        }
        return result;
    }
}
