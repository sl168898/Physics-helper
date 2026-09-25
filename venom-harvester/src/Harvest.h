#pragma once
#include <algorithm>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <vector>

namespace harvest
{
    using ID = std::uint32_t;
    using Recipe = std::vector<ID>;
    struct Ingredient {
        ID form{};
        std::uint32_t count{};
        auto operator<=>(const Ingredient&) const = default;
    };
    using Ingredients = std::vector<Ingredient>;
    constexpr std::uint32_t nonceLimit = 0x00FFFFFF;

    // Read the learned skill at payout, not at brewing. Keep recorded costs
    // unchanged so saved batches still represent their original expenditure.
    constexpr std::uint32_t refundSets(float baseAlchemy)
    {
        return baseAlchemy >= 50.0f ? 2u : 1u;
    }

    inline bool validRecipe(const Recipe& recipe)
    {
        return recipe.size() >= 2 && recipe.size() <= 3 && recipe.front() &&
            std::is_sorted(recipe.begin(), recipe.end()) &&
            std::adjacent_find(recipe.begin(), recipe.end()) == recipe.end();
    }
    inline bool validCost(const Ingredients& cost, const Recipe& recipe)
    {
        if (cost.empty() || cost.size() > recipe.size()) return false;
        ID previous = 0;
        for (const auto& part : cost) {
            if (part.form <= previous || !part.count || part.count > 100000 ||
                !std::binary_search(recipe.begin(), recipe.end(), part.form)) return false;
            previous = part.form;
        }
        return true;
    }

    struct Batch {
        ID poison{};
        std::uint32_t nonce{};
        Recipe recipe;
        Ingredients cost;
        bool paid{};
    };

    // One unique engine-created poison identifies one crafting action. All
    // bottles and weapon hits from that action share the same refund budget.
    class Ledger
    {
    public:
        std::uint32_t sequence{};
        bool armed{}, giftGiven{};
        Recipe stored;
        std::map<ID, Batch> batches;
        std::map<ID, ID> candidates;  // victim -> actual lethal poison
        std::set<ID> rewarded;

        void clear() { *this = {}; }
        bool remember(Recipe recipe)
        {
            std::sort(recipe.begin(), recipe.end());
            if (!validRecipe(recipe)) return false;
            stored = std::move(recipe);
            armed = false;
            return true;
        }
        bool add(Batch batch)
        {
            if ((batch.poison >> 24) != 0xFF || !batch.nonce || batch.nonce > sequence ||
                !validRecipe(batch.recipe) || !validCost(batch.cost, batch.recipe) ||
                batches.contains(batch.poison)) return false;
            batches.emplace(batch.poison, std::move(batch));
            return true;
        }
        bool eligible(ID poison) const
        {
            const auto it = batches.find(poison);
            return it != batches.end() && !it->second.paid && it->second.recipe == stored;
        }
        std::size_t unclaimed() const
        {
            return std::count_if(batches.begin(), batches.end(), [this](const auto& entry) {
                return eligible(entry.first);
            });
        }
        bool offer(ID actor, ID poison)
        {
            if (!actor || rewarded.contains(actor) || !eligible(poison)) return false;
            // First proven lethal health change wins; a later poison ticking
            // on the corpse cannot replace that evidence.
            return candidates.try_emplace(actor, poison).second;
        }
        std::optional<Ingredients> claim(ID actor)
        {
            const auto found = candidates.find(actor);
            if (found == candidates.end()) return std::nullopt;
            const auto poison = found->second;
            candidates.erase(found);
            if (rewarded.contains(actor) || !eligible(poison)) return std::nullopt;
            auto& batch = batches.at(poison);
            batch.paid = true;  // Before an inventory callback can re-enter.
            rewarded.insert(actor);
            return batch.cost;
        }
        template<class ValidIngredient>
        std::optional<Ingredients> claimVerified(ID actor, ValidIngredient validIngredient, float baseAlchemy = 0.0f)
        {
            const auto candidate = candidates.find(actor);
            if (candidate == candidates.end()) return std::nullopt;
            const auto batch = batches.find(candidate->second);
            if (batch == batches.end() || !std::all_of(batch->second.cost.begin(), batch->second.cost.end(),
                    [&](const Ingredient& part) { return validIngredient(part.form); })) {
                candidates.erase(candidate);
                return std::nullopt;
            }
            // offer() already requires a proven lethal poison change. Settling
            // that debt does not require a surviving corpse or another death
            // transition; kDying was sufficient evidence at the native hook.
            auto reward = claim(actor);
            if (reward) for (auto& part : *reward) part.count *= refundSets(baseAlchemy);
            return reward;
        }
        void forgetActor(ID actor)
        {
            // Corpse cleanup must not erase a confirmed, unpaid killing blow.
            // The batch's paid flag remains the duplicate-refund guard.
            rewarded.erase(actor);
        }
    };

    // Explicit little-endian format: no pointers, padding or cached load order.
    inline std::vector<std::uint8_t> encode(const Ledger& ledger)
    {
        std::vector<std::uint8_t> out;
        const auto u32 = [&](ID v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        };
        const auto recipe = [&](const Recipe& r) {
            u32(static_cast<ID>(r.size()));
            for (auto id : r) u32(id);
        };
        u32(2); u32(ledger.sequence); u32(ledger.armed); u32(ledger.giftGiven);
        recipe(ledger.stored);
        u32(static_cast<ID>(ledger.batches.size()));
        for (const auto& [id, batch] : ledger.batches) {
            u32(id); u32(batch.nonce); u32(batch.paid); recipe(batch.recipe);
            u32(static_cast<ID>(batch.cost.size()));
            for (const auto& part : batch.cost) { u32(part.form); u32(part.count); }
        }
        u32(static_cast<ID>(ledger.candidates.size()));
        for (const auto& [actor, poison] : ledger.candidates) { u32(actor); u32(poison); }
        u32(static_cast<ID>(ledger.rewarded.size()));
        for (auto actor : ledger.rewarded) u32(actor);
        return out;
    }

    template<class Resolve>
    inline std::optional<Ledger> decode(std::span<const std::uint8_t> bytes, Resolve resolve)
    {
        std::size_t offset = 0;
        bool good = true;
        const auto u32 = [&]() -> ID {
            if (offset + 4 > bytes.size()) { good = false; return 0; }
            ID v = 0;
            for (int i = 0; i < 4; ++i) v |= static_cast<ID>(bytes[offset++]) << (8 * i);
            return v;
        };
        const auto recipe = [&]() {
            Recipe r;
            const auto count = u32();
            if (count > 3) { good = false; return r; }
            for (ID i = 0; i < count; ++i) r.push_back(resolve(u32()));
            std::sort(r.begin(), r.end());
            return r;
        };
        if (u32() != 2) return std::nullopt;
        Ledger ledger;
        ledger.sequence = u32();
        const auto armed = u32(), gift = u32();
        if (ledger.sequence > nonceLimit || armed > 1 || gift > 1) return std::nullopt;
        ledger.armed = armed != 0; ledger.giftGiven = gift != 0;
        ledger.stored = recipe();
        if (!ledger.stored.empty() && !validRecipe(ledger.stored)) ledger.stored.clear();
        const auto count = u32();
        if (!good || count > 1000000 || count > bytes.size() / 32) return std::nullopt;
        std::set<ID> nonces;
        for (ID i = 0; i < count; ++i) {
            Batch batch;
            batch.poison = resolve(u32()); batch.nonce = u32();
            const auto paid = u32();
            batch.paid = paid != 0; batch.recipe = recipe();
            const auto parts = u32();
            if (!good || paid > 1 || parts > 3 || !batch.nonce || batch.nonce > ledger.sequence ||
                !nonces.insert(batch.nonce).second) return std::nullopt;
            for (ID j = 0; j < parts; ++j) {
                const auto form = resolve(u32()), amount = u32();
                batch.cost.push_back({form, amount});
            }
            std::sort(batch.cost.begin(), batch.cost.end());
            if (batch.poison && validRecipe(batch.recipe) && validCost(batch.cost, batch.recipe)) {
                if (!ledger.add(std::move(batch))) return std::nullopt;
            }
        }
        const auto pending = u32();
        if (!good || pending > 1000000 || pending > bytes.size() / 8) return std::nullopt;
        for (ID i = 0; i < pending; ++i) {
            const auto actor = resolve(u32()), poison = resolve(u32());
            if (actor && ledger.eligible(poison)) ledger.candidates.try_emplace(actor, poison);
        }
        const auto completed = u32();
        if (!good || completed > 1000000 || completed > bytes.size() / 4) return std::nullopt;
        for (ID i = 0; i < completed; ++i) if (auto actor = resolve(u32())) ledger.rewarded.insert(actor);
        if (!good || offset != bytes.size()) return std::nullopt;
        for (auto actor : ledger.rewarded) ledger.candidates.erase(actor);
        return ledger;
    }
}
