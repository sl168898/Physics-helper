#include "PoisonSnapshot.h"
#include <cassert>
#include <iostream>

using pa::crafted::Reason;

std::optional<pa::Key> keyOf(RE::TESForm* form)
{
    if (!form || form->IsDynamicForm() || form->file.empty()) return {};
    pa::Key key{pa::lower(form->file), form->id & 0xFFFFFF};
    return pa::valid(key) ? std::optional(key) : std::nullopt;
}
std::string nameOf(RE::TESForm* form) { return form ? form->name : "Unnamed"; }

struct Fixture
{
    RE::BSScript::IObjectHandlePolicy policy;
    RE::BSScript::Internal::VirtualMachine vm;
    RE::TESDataHandler data;
    RE::TESForm::FormMap allForms;
    RE::AlchemyItem poison;
    RE::EffectSetting weakness, damage;
    RE::BGSKeyword keyword;
    RE::Effect effect, secondary;
    pa::crafted::Issue issue;
    Fixture() {
        vm.policy = &policy; RE::BSScript::Internal::VirtualMachine::instance = &vm;
        RE::TESDataHandler::instance = &data;
        RE::TESForm::allForms = &allForms;
        poison.id = 0xFF000321; poison.name = "Poison of Weakness to Fire";
        weakness.id = 0x73F2E; weakness.name = "Weakness to Fire";
        damage.id = 0x3EB42; damage.name = "Damage Health";
        keyword.id = 0x8CDED; keyword.name = "VendorItemPoison";
        effect.baseEffect = &weakness; effect.effectItem = {42.5f, 0, 30}; effect.cost = 15.75f;
        secondary.baseEffect = &damage; secondary.effectItem = {7.0f, 0, 12}; secondary.cost = 9.5f;
        poison.effects = {&effect, &secondary}; poison.keywords = {&keyword};
    }
    auto capture() { return pa::crafted::capture(&poison, keyOf, nameOf, &issue); }
    void attach(RE::BSScript::ObjectTypeInfo* type, RE::VMHandle handle = 123) {
        auto object = std::make_shared<RE::BSScript::Object>(); object->type = type;
        vm.attachedScripts[handle].push_back(object);
    }
};

int main()
{
    // The real regression: a normal crafted poison has a VM handle, but no
    // attached scripts. The old HasVMAD rejection would reject this fixture.
    {
        Fixture f; assert(f.poison.HasVMAD()); f.poison.misleadingHelperCalls = 0;
        const auto p = f.capture();
        assert(p && p->custom() && f.issue.reason == Reason::none);
        assert(f.poison.misleadingHelperCalls == 0);
        assert(p->effects.size() == 2 && p->effects[0].base.local == f.weakness.id);
        assert(p->effects[0].magnitude == 42.5f && p->effects[0].duration == 30 && p->effects[0].cost == 15.75f);
        assert(p->effects[1].magnitude == 7 && p->effects[1].duration == 12);
        assert(p->keywords.size() == 1 && p->flags == (1u << 17) && p->value == 37);
        pa::Recipes recipes{{{"skyrim.esm", 0x1397E}, *p, "Iron Bolt [Weakness to Fire]"}};
        const auto bytes = pa::encode(recipes);
        f.poison.effects.clear(); f.poison.keywords.clear(); // Original bottle is gone.
        const auto loaded = pa::decode(bytes);
        assert(loaded == recipes && pa::fingerprint(loaded) == pa::fingerprint(recipes));
    }
    for (const auto baseName : {"Potion", "Form", "pOtIoN"}) {
        Fixture f; RE::BSScript::ObjectTypeInfo base{baseName}; f.attach(&base);
        assert(f.capture() && f.issue.reason == Reason::none && RE::scriptLockDepth == 0);
    }
    { Fixture f; f.poison.handle = 0; assert(f.capture()); }
    { Fixture f; f.vm.attachedScripts[123] = {}; assert(f.capture()); }
    { Fixture f; f.vm.attachedScripts[123].push_back(nullptr); assert(f.capture()); }
    { Fixture f; RE::BSScript::ObjectTypeInfo unrelated{"CustomOtherPotion"}; f.attach(&unrelated, 999); assert(f.capture()); }
    // A real custom subclass still cannot silently lose its script state.
    {
        Fixture f; RE::BSScript::ObjectTypeInfo base{"Potion"}, custom{"SpecialPoisonScript"};
        f.attach(&base); f.attach(&custom);
        assert(!f.capture() && f.issue.reason == Reason::customScript);
        assert(f.issue.detail == "SpecialPoisonScript" && RE::scriptLockDepth == 0);
    }
    { Fixture f; f.attach(nullptr); assert(!f.capture() && f.issue.reason == Reason::scriptInspection && RE::scriptLockDepth == 0); }
    { Fixture f; RE::BSScript::ObjectTypeInfo empty{""}; f.attach(&empty); assert(!f.capture() && f.issue.reason == Reason::scriptInspection); }
    { Fixture f; f.vm.policy = nullptr; assert(!f.capture() && f.issue.reason == Reason::scriptInspection); }
    { Fixture f; RE::BSScript::Internal::VirtualMachine::instance = nullptr; assert(!f.capture() && f.issue.reason == Reason::scriptInspection); }
    // Static poison records retain their scripts and conditions by reference.
    {
        Fixture f; f.poison.id = 0x12345; f.effect.conditions.head = &f;
        RE::BSScript::ObjectTypeInfo custom{"StaticScript"}; f.attach(&custom);
        const auto p = f.capture(); assert(p && !p->custom() && p->effects.empty() && p->source.local == 0x12345);
    }
    // Array-registered providers remain supported alongside global-map providers.
    {
        Fixture f; RE::BGSKeyword generated;
        generated.id = 0xFF000200; generated.editorID = "RuntimeFireWeaknessTag";
        f.data.keywords = {&f.keyword, &generated, &generated}; // Same pointer twice is not ambiguous.
        f.poison.keywords.push_back(&generated);
        f.poison.name = "Weapon Oil of Fire Weakness";
        f.effect.effectItem = {139, 0, 120};
        const auto p = f.capture();
        assert(p && p->custom() && f.issue.reason == Reason::none);
        assert(p->keywords.size() == 1 && p->namedKeywords == std::vector<std::string>{"runtimefireweaknesstag"});
        assert(p->effects[0].magnitude == 139 && p->effects[0].duration == 120);
        pa::Recipes recipes{{{"dawnguard.esm", 0xD099}, *p, "Bolt [Fire Weakness]"}};
        const auto saved = pa::encode(recipes);
        assert(saved[4] == 2);
        // Original sample disappears. The provider recreates the same keyword
        // with another FF ID and address on the next launch.
        f.poison.effects.clear(); f.poison.keywords.clear();
        RE::BGSKeyword recreated; recreated.id = 0xFFABCDEF; recreated.editorID = "RUNTIMEFIREWEAKNESSTAG";
        f.data.keywords = {&f.keyword, &recreated};
        const auto restored = pa::decode(saved);
        assert(restored == recipes && pa::fingerprint(restored) == pa::fingerprint(recipes));
        pa::RuntimeKeywords resolver; std::string error;
        assert(resolver.resolve(restored[0].poison.namedKeywords[0], error) == &recreated && error.empty());
        assert(restored[0].poison.effects == p->effects);
        // No global pointer cache: a later load gets its own registry.
        f.data.keywords = {&f.keyword}; pa::RuntimeKeywords missing;
        assert(!missing.resolve(restored[0].poison.namedKeywords[0], error) && error == "named keyword is not registered");
    }
    // Dynamic Tooltips creates LoreBox keywords with the engine form factory
    // and directly sets formEditorID. They need not appear in the keyword array
    // or EditorID map. Reproduce the reported name and 178% / 120s effect.
    {
        Fixture f; RE::BGSKeyword generated;
        generated.id = 0xFF000901; generated.editorID = "LoreBox_quantDTWhoseQuest";
        f.poison.keywords.push_back(&generated);
        f.effect.effectItem = {178, 0, 120};
        f.data.keywords = {&f.keyword};
        RE::TESForm unrelated; unrelated.id = 0x42;
        unrelated.editorID = generated.editorID; // Other form types do not conflict.
        f.allForms = {{generated.id, &generated}, {unrelated.id, &unrelated}, {0x43, nullptr}};
        const auto p = f.capture();
        assert(p && p->namedKeywords == std::vector<std::string>{"lorebox_quantdtwhosequest"});
        assert(p->effects[0].magnitude == 178 && p->effects[0].duration == 120);
        assert(p->effects[1].magnitude == 7 && p->keywords.size() == 1 && RE::formLockDepth == 0);
        pa::Recipes recipes{{{"dawnguard.esm", 0xD099}, *p, "Bolt [Fire Weakness]"}};
        const auto saved = pa::encode(recipes);
        f.poison.effects.clear(); f.poison.keywords.clear(); f.allForms.clear();
        RE::BGSKeyword recreated; recreated.id = 0xFF009999;
        recreated.editorID = "LOREBOX_QUANTDTWHOSEQUEST";
        f.allForms[recreated.id] = &recreated;
        const auto restored = pa::decode(saved);
        pa::RuntimeKeywords resolver; std::string error;
        assert(restored == recipes && pa::fingerprint(restored) == pa::fingerprint(recipes));
        assert(resolver.resolve(restored[0].poison.namedKeywords[0], error) == &recreated && error.empty());
        assert(RE::formLockDepth == 0);
        f.allForms.clear(); pa::RuntimeKeywords missing;
        assert(!missing.resolve(p->namedKeywords[0], error) && error == "named keyword is not registered");
    }
    // The union has no precedence rule: duplicate pointers are harmless, but
    // distinct keywords with the same name across either registry are ambiguous.
    {
        Fixture f; f.keyword.id = 0xFF000200; f.keyword.editorID = "SharedProviderTag";
        f.data.keywords = {&f.keyword, &f.keyword}; f.allForms[f.keyword.id] = &f.keyword;
        assert(f.capture());
        RE::BGSKeyword duplicate; duplicate.id = 0xFF000201; duplicate.editorID = "sharedprovidertag";
        f.allForms[duplicate.id] = &duplicate;
        assert(!f.capture() && f.issue.detail.find("ambiguous") != std::string::npos);
        f.data.keywords.clear();
        assert(!f.capture() && f.issue.detail.find("ambiguous") != std::string::npos);
        f.allForms.erase(f.keyword.id);
        assert(!f.capture() && f.issue.detail.find("different object") != std::string::npos);
        assert(RE::formLockDepth == 0);
    }
    {
        Fixture f; f.keyword.id = 0xFF000200; f.keyword.editorID = "RuntimeTag";
        f.data.keywords = {&f.keyword}; RE::TESForm::allForms = nullptr;
        assert(!f.capture() && f.issue.detail.find("global form registry is unavailable") != std::string::npos);
        assert(RE::formLockDepth == 0);
        RE::TESForm::allForms = &f.allForms;
        pa::RuntimeKeywords resolver; std::string error;
        RE::TESForm::allForms = nullptr; assert(!resolver.resolve("RuntimeTag", error));
        RE::TESForm::allForms = &f.allForms;
        assert(resolver.resolve("RuntimeTag", error) == &f.keyword && error.empty());
    }
    {
        Fixture f; f.keyword.id = 0xFF000200; f.keyword.editorID = "OilTag";
        assert(!f.capture() && f.issue.reason == Reason::keywordIdentity);
        assert(f.issue.detail.find("OilTag") != std::string::npos && f.issue.detail.find("not registered") != std::string::npos);
        f.data.keywords = {&f.keyword}; assert(f.capture());
        RE::BGSKeyword duplicate; duplicate.id = 0xFF000201; duplicate.editorID = "oiltag";
        f.data.keywords.push_back(&duplicate);
        assert(!f.capture() && f.issue.reason == Reason::keywordIdentity && f.issue.detail.find("ambiguous") != std::string::npos);
        f.data.keywords = {&duplicate};
        assert(!f.capture() && f.issue.detail.find("different object") != std::string::npos);
        f.data.keywords = {&f.keyword}; RE::TESDataHandler::instance = nullptr;
        assert(!f.capture() && f.issue.detail.find("registry is unavailable") != std::string::npos);
    }
    {
        Fixture f; f.keyword.id = 0xFF000200;
        f.keyword.editorID.assign(pa::maxKeywordNameBytes + 1, 'x'); f.data.keywords = {&f.keyword};
        assert(!f.capture() && f.issue.reason == Reason::keywordIdentity);
        f.keyword.editorID.assign(pa::maxKeywordNameBytes, 'x'); assert(f.capture());
    }
    // White Phial protected bottles are plugin records. Even with generated
    // keywords they keep the original record, bypassing dynamic snapshots.
    {
        Fixture f; f.poison.id = 0xFE012900; f.poison.file = "White Phial - Decanting.esp";
        f.keyword.id = 0xFF000200; RE::TESDataHandler::instance = nullptr;
        RE::BSScript::Internal::VirtualMachine::instance = nullptr;
        const auto p = f.capture();
        assert(p && !p->custom() && p->source.file == "white phial - decanting.esp" && p->namedKeywords.empty());
    }
    // Distinct diagnostics, all rejected before the inventory transaction.
    { Fixture f; f.poison.poison = false; assert(!f.capture() && f.issue.reason == Reason::notPoison); }
    { Fixture f; f.poison.id = 0x12345; f.poison.file.clear(); assert(!f.capture() && f.issue.reason == Reason::noSource); }
    { Fixture f; f.poison.effects.clear(); assert(!f.capture() && f.issue.reason == Reason::effectCount); }
    { Fixture f; f.poison.effects.resize(65, &f.effect); assert(!f.capture() && f.issue.reason == Reason::effectCount); }
    { Fixture f; f.poison.effects[1] = nullptr; assert(!f.capture() && f.issue.reason == Reason::missingEffect && f.issue.index == 1); }
    { Fixture f; f.effect.baseEffect = nullptr; assert(!f.capture() && f.issue.reason == Reason::missingEffect); }
    { Fixture f; f.effect.conditions.head = &f; assert(!f.capture() && f.issue.reason == Reason::effectConditions && f.issue.related == f.weakness.id); }
    { Fixture f; f.weakness.id = 0xFF000100; assert(!f.capture() && f.issue.reason == Reason::effectSource && f.issue.related == 0xFF000100); }
    { Fixture f; f.weakness.file.clear(); assert(!f.capture() && f.issue.reason == Reason::effectSource); }
    { Fixture f; f.keyword.id = 0xFF000200; assert(!f.capture() && f.issue.reason == Reason::keywordIdentity && f.issue.related == 0xFF000200); }
    { Fixture f; f.poison.keywords[0] = nullptr; assert(!f.capture() && f.issue.reason == Reason::keywordSource); }
    { Fixture f; f.poison.effects.clear(); assert(!f.capture()); f.poison.effects = {&f.effect}; assert(f.capture() && f.issue.reason == Reason::none); }
    std::cout << "PASS: production capture resolves factory-created LoreBox and array-registered keywords, preserves crafted fire-weakness oils across changed FF IDs, retains VM/script safeguards, and accepts White Phial records\n";
}
