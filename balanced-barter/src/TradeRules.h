#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

namespace barter {
constexpr std::int64_t limit = std::numeric_limits<std::int32_t>::max();
constexpr std::size_t maxLines = 64;
struct Line {
    std::uint64_t id{};
    bool buy{};
    std::int32_t count{}, price{}, available{};
};
enum class Error { none, empty, invalid, tooMany, changed, playerGold, vendorGold, overflow };
struct Settlement {
    Error error{};
    std::int64_t purchases{}, sales{}, net{}, playerAfter{}, vendorAfter{};
    explicit operator bool() const { return error == Error::none; }
};
inline std::string_view message(Error error) {
    switch (error) {
    case Error::none: return "Ready to exchange";
    case Error::empty: return "Select items to build an offer";
    case Error::invalid: return "Invalid quantity or price";
    case Error::tooMany: return "An offer can contain at most 64 different entries";
    case Error::changed: return "An item changed. Clear the offer and select it again";
    case Error::playerGold: return "You need more gold for the difference";
    case Error::vendorGold: return "The merchant needs more gold for the difference";
    case Error::overflow: return "This exchange exceeds Skyrim's gold limit";
    }
    return "Exchange unavailable";
}
inline Settlement evaluate(std::span<const Line> lines, std::int64_t playerGold, std::int64_t vendorGold) {
    Settlement out;
    auto fail = [&](Error e) { out.error = e; return out; };
    if (lines.empty()) return fail(Error::empty);
    if (lines.size() > maxLines) return fail(Error::tooMany);
    if (playerGold < 0 || vendorGold < 0 || playerGold > limit || vendorGold > limit) return fail(Error::overflow);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        if (!line.id || line.count <= 0 || line.price < 0 || line.available < 0) return fail(Error::invalid);
        if (line.count > line.available) return fail(Error::changed);
        for (std::size_t j = 0; j < i; ++j) if (lines[j].id == line.id) return fail(Error::invalid);
        auto& total = line.buy ? out.purchases : out.sales;
        total += std::int64_t(line.count) * line.price;
        // Reject oversized offers before they enter any engine int32 API.
        if (total > limit) return fail(Error::overflow);
    }
    out.net = out.purchases - out.sales;
    out.playerAfter = playerGold - out.net;
    out.vendorAfter = vendorGold + out.net;
    if (out.playerAfter < 0) return fail(Error::playerGold);
    if (out.vendorAfter < 0) return fail(Error::vendorGold);
    if (out.playerAfter > limit || out.vendorAfter > limit) return fail(Error::overflow);
    return out;
}
// The inventory adapter supplies validation, transfer, and reversal. No gold is
// transferred unless every item move succeeded. Successful steps are unwound
// in reverse order on failure; never manufacture replacement items or gold.
template<class Adapter>
bool transact(Adapter& adapter, std::size_t steps) {
    if (!adapter.validate()) return false;
    std::size_t completed = 0;
    for (; completed < steps; ++completed) {
        if (!adapter.move(completed)) {
            adapter.recoverFailed(completed);
            while (completed) adapter.undo(--completed);
            return false;
        }
    }
    if (!adapter.pay()) {
        adapter.refund();
        while (completed) adapter.undo(--completed);
        return false;
    }
    return true;
}
}
