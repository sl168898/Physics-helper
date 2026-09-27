#pragma once

#include <cstddef>

namespace no_jarrin
{
    // Keep every index, allocation, and script-added count intact. A persistent
    // non-inventory quest occupies the forbidden seed's slot. The crop list is
    // deliberately untouched so all other seed/crop pairings stay as they were.
    template <class Range, class Value>
    std::size_t blockSlots(Range& entries, Value forbidden, Value marker)
    {
        if (forbidden == marker) return 0;
        std::size_t changed = 0;
        for (auto& entry : entries) {
            if (entry == forbidden) {
                entry = marker;
                ++changed;
            }
        }
        return changed;
    }
}
