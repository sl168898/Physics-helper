#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace harvest
{
    // Own native references by ID, not by a raw pointer retained across loads.
    // Policy supplies generation(), acquire(id), and release(id). A new game
    // or load destroys the old native world; stale cleanup must not release a
    // newly loaded form that happens to reuse the same dynamic ID.
    template<class Policy> class DeferredForms final
    {
        std::uint64_t generation_;
        std::vector<std::uint32_t> forms_;
        explicit DeferredForms(std::uint64_t generation) : generation_(generation) {}

    public:
        DeferredForms(const DeferredForms&) = delete;
        DeferredForms& operator=(const DeferredForms&) = delete;

        static std::shared_ptr<DeferredForms> retain(
            std::uint64_t generation, std::span<const std::uint32_t> forms)
        {
            if (forms.empty() || generation != Policy::generation()) return {};
            auto lease = std::shared_ptr<DeferredForms>(new DeferredForms(generation));
            lease->forms_.reserve(forms.size()); // Allocate before acquiring refs.
            for (auto id : forms) {
                if (!id || generation != Policy::generation()) return {};
                if (std::find(lease->forms_.begin(), lease->forms_.end(), id) != lease->forms_.end()) continue;
                if (!Policy::acquire(id)) return {}; // Rolls back earlier refs.
                lease->forms_.push_back(id);
            }
            return lease;
        }

        ~DeferredForms()
        {
            for (auto id : forms_) {
                if (generation_ != Policy::generation()) break;
                Policy::release(id);
            }
        }
    };

    // Mutations synchronously enqueue inventory-event tasks. Put our cleanup
    // AFTER those tasks on the same FIFO queue, with ownership held throughout
    // the mutation and until the cleanup task is disposed. This is a queue
    // ordering guarantee, not an arbitrary delay or a permanent form pin.
    template<class Lease, class Mutate, class Queue>
    bool retainAcrossQueuedEvents(std::shared_ptr<Lease> lease, Mutate&& mutate, Queue&& queue)
    {
        if (!lease) return false;
        std::forward<Mutate>(mutate)();
        std::forward<Queue>(queue)([lease = std::move(lease)] {});
        return true;
    }
}
