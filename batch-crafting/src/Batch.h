#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
namespace batch {
constexpr std::uint32_t limit=1000;
struct Material { std::uint32_t id; std::int64_t cost, available; };
inline std::uint32_t maximum(const std::vector<Material>& materials, std::uint32_t output) {
    if (materials.empty()) return 0; // No unlimited/free recipe automation.
    std::map<std::uint32_t,std::pair<std::int64_t,std::int64_t>> totals;
    for (const auto& m : materials) {
        if (!m.id || m.id==output || m.cost<=0 || m.available<0) return 0;
        auto [it,inserted]=totals.try_emplace(m.id,0,m.available);
        if (!inserted && it->second.second!=m.available) return 0;
        if (m.cost>std::numeric_limits<std::int64_t>::max()-it->second.first) return 0;
        it->second.first+=m.cost;
    }
    std::int64_t n=limit;
    for (const auto& [id,v] : totals) n=std::min(n,v.second/v.first);
    return static_cast<std::uint32_t>(n);
}
struct Selection {
    std::uint64_t session{};
    std::uintptr_t menu{},sub{};
    std::uint32_t recipe{}, output{}, units{};
    bool operator==(const Selection&) const=default;
};
struct Job {
    Selection selection{};
    std::uint32_t requested{},completed{};
    bool active{},awaiting{};
    std::int64_t before{};
    bool start(Selection s,std::uint32_t count,std::uint32_t available) {
        if(active || !s.recipe || !s.output || !s.units || !count || count>available || count>limit) return false;
        *this={s,count,0,true,false,0};return true;
    }
    bool canCraft(Selection s,std::uint32_t available) const {
        return active && !awaiting && s==selection && completed<requested && available>0;
    }
    void submitted(std::int64_t count) { before=count; awaiting=true; }
    bool confirm(std::int64_t count) {
        if(!active || !awaiting || count<before || count-before<selection.units) { stop();return false; }
        awaiting=false; ++completed;
        if(completed>=requested)active=false;
        return true;
    }
    void stop(){active=false;awaiting=false;}
};
}
