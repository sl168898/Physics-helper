// Test doubles for the production effect adapter; native builds also compile
// that same adapter against the pinned CommonLib headers.
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>
namespace RE {
enum class ActorValue { kNone, kHealth, kResistFire, kResistFrost, kResistShock, kPoisonResist, kParalysis };
enum class EffectArchetype { kValueModifier, kDualValueModifier };
struct EffectSetting {
    struct EffectSettingData {
        enum class Flag { kNoMagnitude, kNoDuration };
        struct Flags { bool all(Flag) const { return false; } } flags;
        ActorValue primaryAV = ActorValue::kNone, secondaryAV = ActorValue::kNone, resistVariable = ActorValue::kNone;
        EffectArchetype archetype = EffectArchetype::kValueModifier;
    } data;
    bool hostile = false, detrimental = false;
    std::vector<std::string_view> keywords;
    bool IsHostile() const { return hostile; }
    bool IsDetrimental() const { return detrimental; }
    bool HasKeywordString(std::string_view key) { for (auto k : keywords) if (k == key) return true; return false; }
};
struct Effect {
    EffectSetting* baseEffect{};
    struct Item { float magnitude{}; std::uint32_t duration{}; } effectItem;
};
struct MagicItem { std::vector<Effect*> effects; };
struct BGSExplosion { MagicItem* formEnchanting{}; };
struct BGSProjectileData {
    enum class BGSProjectileFlags { kExplosion };
    struct Flags { bool enabled{}; bool all(BGSProjectileFlags) const { return enabled; } } flags;
    BGSExplosion* explosionType{};
};
struct BGSProjectile { BGSProjectileData data; };
struct TESAmmo {
    struct Runtime { struct Data { BGSProjectile* projectile{}; } data; } runtime;
    Runtime& GetRuntimeData() { return runtime; }
};
}
