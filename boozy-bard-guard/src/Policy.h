#pragma once
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
namespace boozy {
struct EffectView { bool protectedEffect{}, dispelled{}; float elapsed{}, duration{}; };
// A pending/inactive native effect still reserves the slot: it can become active.
inline bool occupies(const EffectView& e) {
    return e.protectedEffect && !e.dispelled && std::isfinite(e.elapsed) &&
        std::isfinite(e.duration) && (e.duration<=0 || e.elapsed<e.duration);
}
inline std::optional<std::size_t> oldest(std::span<const EffectView> effects) {
    std::optional<std::size_t> selected;
    for(std::size_t i=0;i<effects.size();++i)
        if(occupies(effects[i]) && (!selected || effects[i].elapsed>effects[*selected].elapsed)) selected=i;
    return selected;
}
inline bool blocked(bool protectedIncoming,bool pending,std::span<const EffectView> effects) {
    return protectedIncoming && (pending || oldest(effects).has_value());
}
}
