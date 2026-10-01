#pragma once
#include <atomic>
#include <cstdint>
#include <optional>

namespace harvest {
struct MenuRequest {
    std::uint64_t generation{}, ticket{};
    bool cancel{};
};

// A copied request can outlive its UI callback. Old or duplicate callbacks
// cannot change the ledger or unlock a newer menu after save/load.
class MenuGate {
    std::atomic<std::uint64_t> sequence{}, current{};
public:
    std::uint64_t begin() {
        const auto ticket = sequence.fetch_add(1) + 1;
        std::uint64_t empty = 0;
        return ticket && current.compare_exchange_strong(empty, ticket) ? ticket : 0;
    }
    bool finish(std::uint64_t ticket) {
        return ticket && current.compare_exchange_strong(ticket, 0);
    }
    std::optional<bool> resolve(MenuRequest request, std::uint64_t generation,
        unsigned button, bool enabled) {
        if (request.generation != generation || !finish(request.ticket) ||
            button != 0 || !enabled) return std::nullopt;
        return !request.cancel;
    }
    void reset() { current.store(0); }
};
}
