#pragma once
#include "CraftCapture.h"
#include "BatchNames.h"
#include <limits>

namespace harvest
{
    struct PendingCraft
    {
        ID source{};
        std::uint32_t nonce{}, bottles{}, reserve{};
        Recipe recipe;
        Ingredients cost;
        std::string customName;
        bool nameKnown{};
        std::uint32_t nameReserve{};
    };

    // Capture expenditure at the crafting callback, but do not replace items
    // while a menu or another mod can hold borrowed inventory entries.
    class PendingCrafts
    {
        std::vector<PendingCraft> entries_;
    public:
        const auto& entries() const { return entries_; }
        bool empty() const { return entries_.empty(); }
        void clear() { entries_.clear(); }

        bool add(PendingCraft value, std::int32_t countBefore)
        {
            if (!value.source || !value.nonce || value.nonce > nonceLimit || !value.bottles ||
                value.bottles > INT32_MAX || countBefore < 0 ||
                !validRecipe(value.recipe) || !validCost(value.cost, value.recipe) ||
                !validBatchName(value.customName) || value.nameReserve > INT32_MAX ||
                value.nameReserve > static_cast<std::uint32_t>(countBefore) ||
                (!value.nameKnown && (!value.customName.empty() || value.nameReserve))) return false;
            value.reserve = static_cast<std::uint32_t>(countBefore);
            std::uint64_t expected = 0;
            bool found = false;
            bool foundName = false;
            std::uint64_t expectedName = 0;
            const auto countBeforeName = value.nameReserve;
            for (const auto& prior : entries_) {
                if (prior.nonce == value.nonce) return false;
                if (prior.source != value.source) continue;
                if (value.nameKnown && prior.nameKnown && prior.customName == value.customName) {
                    if (!foundName) { expectedName = value.nameReserve = prior.nameReserve; foundName = true; }
                    expectedName += prior.bottles;
                }
                if (!found) { expected = value.reserve = prior.reserve; found = true; }
                expected += prior.bottles;
            }
            if ((foundName && expectedName > countBeforeName) ||
                (found && expected > static_cast<std::uint64_t>(countBefore)) ||
                static_cast<std::uint64_t>(countBefore) + value.bottles > INT32_MAX) return false;
            entries_.push_back(std::move(value));
            return true;
        }

        // Calling this during a closing animation or a newly reopened menu
        // leaves every batch pending; a later close/poll retries it.
        std::vector<PendingCraft> takeWhenClosed(bool menuExists)
        {
            if (menuExists) return {};
            auto result = std::move(entries_);
            entries_.clear();
            return result;
        }
    };

    inline std::map<ID, std::uint64_t> requiredSourceCounts(std::span<const PendingCraft> pending)
    {
        std::map<ID, std::uint64_t> result;
        for (const auto& entry : pending) {
            const auto [it, inserted] = result.try_emplace(entry.source, entry.reserve);
            it->second += entry.bottles;
        }
        return result;
    }

    // Optional extra co-save record. HSAT v2 and existing bound batches are
    // unchanged. A save made before closing the alchemy menu retains its exact
    // captured costs; loading never invents a budget from inventory contents.
    inline std::vector<std::uint8_t> encodePending(const PendingCrafts& pending)
    {
        std::vector<std::uint8_t> out;
        const auto put = [&](std::uint32_t v) {
            for (int i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(v >> (8 * i)));
        };
        put(2); put(static_cast<std::uint32_t>(pending.entries().size()));
        for (const auto& entry : pending.entries()) {
            put(entry.source); put(entry.nonce); put(entry.bottles); put(entry.reserve);
            put(static_cast<std::uint32_t>(entry.recipe.size()));
            for (auto id : entry.recipe) put(id);
            put(static_cast<std::uint32_t>(entry.cost.size()));
            for (const auto& part : entry.cost) { put(part.form); put(part.count); }
            put(entry.nameKnown ? 1 : 0); put(entry.nameReserve);
            put(static_cast<std::uint32_t>(entry.customName.size()));
            out.insert(out.end(), entry.customName.begin(), entry.customName.end());
        }
        return out;
    }

    template<class Resolve>
    std::optional<PendingCrafts> decodePending(std::span<const std::uint8_t> bytes, Resolve resolve)
    {
        std::size_t offset = 0;
        bool good = true;
        const auto get = [&]() -> std::uint32_t {
            if (offset + 4 > bytes.size()) { good = false; return 0; }
            std::uint32_t v = 0;
            for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(bytes[offset++]) << (8 * i);
            return v;
        };
        const auto version = get();
        if (version != 1 && version != 2) return std::nullopt;
        const auto count = get();
        if (!good || count > 1000000 || count > bytes.size() / 40) return std::nullopt;
        PendingCrafts result;
        std::set<ID> nonces;
        std::map<ID, std::uint64_t> used;
        std::map<ID, std::uint32_t> reserves;
        std::map<std::pair<ID, std::string>, std::uint64_t> namedUsed;
        std::map<std::pair<ID, std::string>, std::uint32_t> namedReserves;
        for (ID i = 0; i < count; ++i) {
            PendingCraft entry;
            entry.source = resolve(get()); entry.nonce = get(); entry.bottles = get(); entry.reserve = get();
            const auto recipeSize = get();
            if (!good || recipeSize < 2 || recipeSize > 3 || !entry.nonce || entry.nonce > nonceLimit ||
                !entry.bottles || entry.bottles > INT32_MAX || entry.reserve > INT32_MAX ||
                !nonces.insert(entry.nonce).second) return std::nullopt;
            for (ID j = 0; j < recipeSize; ++j) entry.recipe.push_back(resolve(get()));
            const auto parts = get();
            if (!good || parts < 1 || parts > recipeSize) return std::nullopt;
            for (ID j = 0; j < parts; ++j) {
                const auto form = resolve(get()), amount = get();
                entry.cost.push_back({form, amount});
            }
            if (version == 2) {
                const auto known = get(); entry.nameReserve = get();
                const auto size = get();
                if (!good || known > 1 || size > maxBatchNameBytes ||
                    entry.nameReserve > INT32_MAX || size > bytes.size() - offset) return std::nullopt;
                entry.nameKnown = known != 0;
                entry.customName.assign(reinterpret_cast<const char*>(bytes.data() + offset), size);
                offset += size;
                if (!validBatchName(entry.customName)) return std::nullopt;
            }
            std::sort(entry.recipe.begin(), entry.recipe.end());
            std::sort(entry.cost.begin(), entry.cost.end());
            if (!good) return std::nullopt;
            if (!entry.source || !validRecipe(entry.recipe) || !validCost(entry.cost, entry.recipe)) continue;
            const auto [reserve, inserted] = reserves.try_emplace(entry.source, entry.reserve);
            if (!inserted && reserve->second != entry.reserve) return std::nullopt;
            const auto before = static_cast<std::uint64_t>(entry.reserve) + used[entry.source];
            const auto key = std::pair{entry.source, entry.customName};
            if (entry.nameKnown) {
                const auto [named, fresh] = namedReserves.try_emplace(key, entry.nameReserve);
                if (!fresh && named->second != entry.nameReserve) return std::nullopt;
                const auto namedBefore = static_cast<std::uint64_t>(entry.nameReserve) + namedUsed[key];
                if (namedBefore > INT32_MAX) return std::nullopt;
                entry.nameReserve = static_cast<std::uint32_t>(namedBefore);
            }
            if (before > INT32_MAX || !result.add(entry, static_cast<std::int32_t>(before))) return std::nullopt;
            used[entry.source] += entry.bottles;
            if (entry.nameKnown) namedUsed[key] += entry.bottles;
        }
        if (!good || offset != bytes.size()) return std::nullopt;
        return result;
    }
}
