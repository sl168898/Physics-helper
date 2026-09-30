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
    RE::AlchemyItem poison;
    RE::EffectSetting weakness, damage;
    RE::BGSKeyword keyword;
    RE::Effect effect, secondary;
    pa::crafted::Issue issue;
    Fixture() {
        vm.policy = &policy; RE::BSScript::Internal::VirtualMachine::instance = &vm;
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
    { Fixture f; f.keyword.id = 0xFF000200; assert(!f.capture() && f.issue.reason == Reason::keywordSource && f.issue.related == 0xFF000200); }
    { Fixture f; f.poison.keywords[0] = nullptr; assert(!f.capture() && f.issue.reason == Reason::keywordSource); }
    { Fixture f; f.poison.effects.clear(); assert(!f.capture()); f.poison.effects = {&f.effect}; assert(f.capture() && f.issue.reason == Reason::none); }
    std::cout << "PASS: production crafted-poison capture accepts ordinary VM handles/native wrappers, preserves weakness and mixed effects across save/load, and identifies retained rejection cases\n";
}
