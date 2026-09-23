// Exercise the real grant controller against a queued VM/game-task test double.
// This checks control flow, not Skyrim's native implementation of its functions.
// g++ -std=c++23 grant_lifecycle_test.cpp -I ../three-traits/native/src -o grant_test
#include <array>
#include <cassert>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace RE {
template<class T> using BSTSmartPointer = std::shared_ptr<T>;
template<class T, class... A> auto make_smart(A&&... a) { return std::make_shared<T>(std::forward<A>(a)...); }
struct TESWordOfPower {
    std::uint32_t id;
    bool known = false, unlocked = false;
    bool GetKnown() const { return known; }
    std::uint32_t GetFormID() const { return id; }
};
struct TESShout { struct Word { TESWordOfPower* word; }; std::array<Word, 3> variations; };
struct PlayerCharacter {
    bool owns = false, dead = false;
    int adds = 0;
    static PlayerCharacter* GetSingleton() { static PlayerCharacter player; return &player; }
    bool IsDead() const { return dead; }
    bool HasShout(TESShout*) const { return owns; }
    bool AddShout(TESShout*) { ++adds; return owns = true; }
};
inline int notices = 0;
void DebugNotification(const char*) { ++notices; }
namespace BSScript {
struct Object {};
struct Variable { bool isBool = false, result = false; bool IsBool() const { return isBool; } bool GetBool() const { return result; } };
struct IStackCallbackFunctor {
    virtual ~IStackCallbackFunctor() = default;
    virtual void operator()(Variable) = 0;
    virtual void SetObject(const BSTSmartPointer<Object>&) = 0;
};
struct IFunctionArguments { TESWordOfPower* word; };
namespace Internal {
struct VirtualMachine {
    struct Request { std::string name; TESWordOfPower* word; BSTSmartPointer<IStackCallbackFunctor> callback; };
    std::deque<Request> queue;
    bool reject = false;
    int taught = 0, unlocked = 0, queried = 0;
    static VirtualMachine* GetSingleton() { static VirtualMachine vm; return &vm; }
    bool DispatchStaticCall(const char* script, const char* method, IFunctionArguments* args, BSTSmartPointer<IStackCallbackFunctor> callback) {
        assert(std::string(script) == "Game");
        if (reject) return false;
        queue.push_back({method, args->word, std::move(callback)});
        return true;
    }
    void step() {
        assert(!queue.empty());
        auto item = std::move(queue.front()); queue.pop_front();
        Variable value;
        if (item.name == "IsWordUnlocked") { ++queried; value = {true, item.word->unlocked}; }
        else if (item.name == "TeachWord") { ++taught; item.word->known = true; }
        else if (item.name == "UnlockWord") { ++unlocked; assert(item.word->known); item.word->unlocked = true; }
        else assert(false);
        (*item.callback)(value);
    }
};
}
}
BSScript::IFunctionArguments* MakeFunctionArguments(TESWordOfPower* word) { return new BSScript::IFunctionArguments{word}; }
}
namespace SKSE {
struct Tasks {
    std::deque<std::function<void()>> queue;
    void AddTask(std::function<void()> task) { queue.push_back(std::move(task)); }
    void step() { auto task = std::move(queue.front()); queue.pop_front(); task(); }
};
Tasks* GetTaskInterface() { static Tasks tasks; return &tasks; }
namespace log {
template<class... A> void info(const char*, A&&...) {}
template<class... A> void warn(const char*, A&&...) {}
}
}

#include "IronLungsGrant.h"

using VM = RE::BSScript::Internal::VirtualMachine;
struct Fixture {
    std::array<RE::TESWordOfPower, 3> words{{{0x13e22}, {0x13e23}, {0x13e24}}};
    RE::TESShout shout{{{{&words[0]}, {&words[1]}, {&words[2]}}}};
    traits::IronLungsGrant grant;
    bool selected = true;
    Fixture() {
        *RE::PlayerCharacter::GetSingleton() = {};
        *VM::GetSingleton() = {};
        SKSE::GetTaskInterface()->queue.clear(); RE::notices = 0;
        grant.init(&shout, [this] { return selected; });
    }
    void flush() {
        auto vm = VM::GetSingleton(); auto tasks = SKSE::GetTaskInterface();
        int budget = 100;
        while (!vm->queue.empty() || !tasks->queue.empty()) {
            assert(--budget > 0);
            if (!vm->queue.empty()) vm->step();
            if (!tasks->queue.empty()) tasks->step();
        }
    }
    void allUnlocked() const { for (const auto& word : words) assert(word.known && word.unlocked); }
};

int main() {
    {
        Fixture f;
        f.selected = false; f.grant.tick(1); f.flush();
        assert(!RE::PlayerCharacter::GetSingleton()->owns && VM::GetSingleton()->queried == 0);
        f.selected = true; f.grant.tick(1); f.flush(); f.allUnlocked();
        assert(VM::GetSingleton()->taught == 3 && VM::GetSingleton()->unlocked == 3);
        assert(RE::PlayerCharacter::GetSingleton()->adds == 1 && RE::notices == 1);
        for (int i = 0; i < 100; ++i) f.grant.tick(1);
        f.flush();
        assert(VM::GetSingleton()->taught == 3 && VM::GetSingleton()->unlocked == 3);
        f.grant.reset(); f.grant.tick(1); f.flush(); f.allUnlocked();
        assert(VM::GetSingleton()->taught == 3 && VM::GetSingleton()->unlocked == 3 && RE::notices == 1);
    }
    {
        Fixture f;
        f.words[0].known = f.words[0].unlocked = true;
        f.words[1].known = true;
        f.grant.tick(1); f.flush(); f.allUnlocked();
        assert(VM::GetSingleton()->taught == 1 && VM::GetSingleton()->unlocked == 2);
    }
    {
        Fixture f;
        f.grant.tick(1);
        VM::GetSingleton()->step(); // old save's query completed; callback is queued
        f.grant.reset(); f.selected = false;
        f.flush();
        assert(VM::GetSingleton()->taught == 0 && VM::GetSingleton()->unlocked == 0);
        f.selected = true; f.grant.tick(1); f.flush(); f.allUnlocked();
        assert(VM::GetSingleton()->taught == 3 && VM::GetSingleton()->unlocked == 3);
    }
    {
        Fixture f;
        f.grant.tick(1);
        VM::GetSingleton()->step();
        f.selected = false; f.grant.tick(1); f.flush();
        assert(VM::GetSingleton()->taught == 0 && VM::GetSingleton()->unlocked == 0);
        f.selected = true; f.grant.tick(1); f.flush(); f.allUnlocked();
    }
    {
        Fixture f;
        VM::GetSingleton()->reject = true; f.grant.tick(1);
        VM::GetSingleton()->reject = false; f.grant.tick(4);
        assert(VM::GetSingleton()->queue.empty());
        f.grant.tick(1); f.flush(); f.allUnlocked();
    }
    {
        Fixture f;
        f.grant.tick(1); // delay the first response long enough to trigger retry
        f.grant.tick(31); f.grant.tick(5); f.flush(); f.allUnlocked();
        assert(VM::GetSingleton()->taught == 3 && VM::GetSingleton()->unlocked == 3);
    }
    std::cout << "Grant lifecycle: inactive, new selection, partial knowledge, repeat load, stale callback, removal, failure and timeout scenarios passed.\n";
}
