// Minimal engine doubles for compiling the actual PoisonSnapshot.h adapter.
#pragma once
#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RE
{
    using FormID = std::uint32_t;
    using VMHandle = std::uint64_t;
    inline int scriptLockDepth = 0;
    struct BSSpinLock {};
    struct BSSpinLockGuard {
        explicit BSSpinLockGuard(BSSpinLock&) { ++scriptLockDepth; }
        ~BSSpinLockGuard() { --scriptLockDepth; }
    };
    struct TESForm {
        FormID id{};
        std::string file = "Skyrim.esm", name, editorID;
        VMHandle handle = 123;
        mutable int misleadingHelperCalls = 0;
        FormID GetFormID() const { return id; }
        bool IsDynamicForm() const { return id >= 0xFF000000; }
        int GetFormType() const { return 46; }
        const char* GetFormEditorID() const { return editorID.c_str(); }
        // Same distinction as the pinned helper: handle existence != scripts.
        bool HasVMAD() const { ++misleadingHelperCalls; return handle != 0; }
    };
    struct EffectSetting : TESForm {};
    struct BGSKeyword : TESForm {};
    struct TESDataHandler {
        static inline TESDataHandler* instance{};
        std::vector<BGSKeyword*> keywords;
        static TESDataHandler* GetSingleton() { return instance; }
        template<class T> const auto& GetFormArray() const { return keywords; }
    };
    struct Effect {
        EffectSetting* baseEffect{};
        struct { float magnitude{}; std::uint32_t area{}, duration{}; } effectItem;
        float cost{};
        struct { void* head{}; } conditions;
    };
    struct AlchemyItem : TESForm {
        bool poison = true;
        struct Data {
            std::int32_t costOverride = 37;
            struct Flags { std::uint32_t value = 1u << 17; std::uint32_t underlying() const { return value; } } flags;
        } data;
        std::vector<Effect*> effects;
        std::vector<BGSKeyword*> keywords;
        bool IsPoison() const { return poison; }
        const auto& GetKeywords() const { return keywords; }
    };
    namespace BSScript {
        struct ObjectTypeInfo {
            std::string name;
            const char* GetName() const { return name.c_str(); }
        };
        struct Object {
            ObjectTypeInfo* type{};
            const ObjectTypeInfo* GetTypeInfo() const { assert(scriptLockDepth > 0); return type; }
        };
        struct IObjectHandlePolicy {
            VMHandle GetHandleForObject(int, const TESForm* form) { return form->handle; }
            VMHandle EmptyHandle() const { return 0; }
        };
        namespace Internal {
            struct VirtualMachine {
                static inline VirtualMachine* instance{};
                IObjectHandlePolicy* policy{};
                BSSpinLock attachedScriptsLock;
                std::map<VMHandle, std::vector<std::shared_ptr<Object>>> attachedScripts;
                static VirtualMachine* GetSingleton() { return instance; }
                IObjectHandlePolicy* GetObjectHandlePolicy() { return policy; }
            };
        }
    }
}
