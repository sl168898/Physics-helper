#pragma once
#include <cstdint>
#include <memory>

namespace harvest
{
    // AddObjectToContainer adds inventory entries, but does not grant the
    // created-poison manager its per-bottle references. Reserve those native
    // references before adding, then transfer them to normal engine ownership.
    // An abandoned reservation rolls back; a committed one is NOT a plugin pin.
    template<class Policy> class InventoryBottleRefs final
    {
        std::uint64_t generation_;
        std::uint32_t form_;
        std::uint32_t owned_{};
        InventoryBottleRefs(std::uint64_t generation, std::uint32_t form) :
            generation_(generation), form_(form) {}

    public:
        InventoryBottleRefs(const InventoryBottleRefs&) = delete;
        InventoryBottleRefs& operator=(const InventoryBottleRefs&) = delete;

        static std::unique_ptr<InventoryBottleRefs> retain(
            std::uint64_t generation, std::uint32_t form, std::uint32_t bottles)
        {
            if (!form || !bottles || bottles > 100000 || generation != Policy::generation()) return {};
            auto refs = std::unique_ptr<InventoryBottleRefs>(new InventoryBottleRefs(generation, form));
            for (std::uint32_t i = 0; i < bottles; ++i) {
                if (generation != Policy::generation() || !Policy::acquire(form)) return {};
                ++refs->owned_;
            }
            return refs;
        }

        // Call only once AddObjectToContainer has returned. The inventory's
        // normal consumption/removal lifecycle now owns these references.
        void transferToInventory() { owned_ = 0; }

        ~InventoryBottleRefs()
        {
            while (owned_ && generation_ == Policy::generation()) {
                --owned_;
                Policy::release(form_);
            }
        }
    };
}
