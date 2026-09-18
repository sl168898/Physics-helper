// Regression for the MSVC x64 hidden result argument in RemoveItem hooks.
// Exercise an actual virtual call through a replacement vtable, independent
// of Skyrim, so swapping the implicit result buffer and this fails the test.
#include <cassert>
#include <cstdint>
#include <type_traits>

struct Handle {
    std::uint32_t value{};
    ~Handle() { value = 0; }
};
static_assert(sizeof(Handle) == 4 && !std::is_trivially_destructible_v<Handle>);
struct Actor {
    std::uint32_t marker = 0x12345678;
    virtual Handle remove(void* item, std::int32_t count, void* extra, void* destination) {
        assert(item == reinterpret_cast<void*>(0xAABB));
        assert(count == 7 && extra == reinterpret_cast<void*>(0xCCDD));
        assert(destination == reinterpret_cast<void*>(0xEEFF));
        return {marker};
    }
};
using Remove = Handle* (*)(Actor*, Handle*, void*, std::int32_t, void*, void*);
Remove original;
bool reached = false;
Handle* thunk(Actor* actor, Handle* result, void* item, std::int32_t count, void* extra, void* destination) {
    assert(actor->marker == 0x12345678 && result);
    reached = true;
    return original(actor, result, item, count, extra, destination);
}
__declspec(noinline) Handle invoke(Actor* actor) {
    return actor->remove(reinterpret_cast<void*>(0xAABB), 7,
        reinterpret_cast<void*>(0xCCDD), reinterpret_cast<void*>(0xEEFF));
}
int main() {
    Actor actor;
    auto& table = *reinterpret_cast<void***>(&actor);
    auto saved = table;
    original = reinterpret_cast<Remove>(table[0]);
    void* replacement[] = {reinterpret_cast<void*>(&thunk)};
    table = replacement;
    auto result = invoke(&actor);
    table = saved;
    assert(reached && result.value == actor.marker);
}
