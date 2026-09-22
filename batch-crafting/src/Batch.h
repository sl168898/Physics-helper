#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <vector>

namespace batch {
constexpr std::uint32_t limit=1000;
constexpr auto engineLimit=std::numeric_limits<std::int32_t>::max();
struct Material { std::uint32_t id; std::int64_t cost,available; };
struct Selection {
    std::uint64_t session{};
    std::uintptr_t menu{},sub{};
    std::uint32_t recipe{},output{},units{};
    bool operator==(const Selection&) const=default;
};
struct Cost { std::int64_t each{},available{},seen{}; };
inline std::optional<std::map<std::uint32_t,Cost>> combine(const std::vector<Material>& materials,std::uint32_t output) {
    if(materials.empty() || !output)return {};
    std::map<std::uint32_t,Cost> result;
    for(const auto& m:materials) {
        if(!m.id || m.id==output || m.cost<=0 || m.cost>engineLimit || m.available<0)return {};
        auto [it,added]=result.try_emplace(m.id,Cost{0,m.available,0});
        if((!added && it->second.available!=m.available) || m.cost>engineLimit-it->second.each)return {};
        it->second.each+=m.cost;
    }
    return result;
}
inline std::uint32_t maximum(const std::vector<Material>& materials,std::uint32_t output,std::uint32_t units) {
    auto costs=combine(materials,output);
    if(!costs || !units || units>engineLimit)return 0;
    std::int64_t n=std::min<std::int64_t>(limit,engineLimit/units);
    for(const auto& [id,c]:*costs)n=std::min({n,c.available/c.each,engineLimit/c.each});
    return static_cast<std::uint32_t>(n);
}
// Quotas describe ONE native recipe execution. Only its own inventory calls are
// scaled; nested callbacks are excluded by the adapter's thread-local suspension.
class Transaction {
public:
    Selection selection{};
    std::uint32_t quantity{},outputSeen{},removalCalls{},outputCalls{},skillCalls{};
    std::map<std::uint32_t,Cost> costs;
    static std::optional<Transaction> create(Selection s,std::uint32_t n,const std::vector<Material>& materials) {
        if(!s.recipe || !n || n>maximum(materials,s.output,s.units))return {};
        Transaction t;t.selection=s;t.quantity=n;t.costs=*combine(materials,s.output);return t;
    }
    std::int32_t removal(std::uint32_t id,std::int32_t count) {
        auto it=costs.find(id);
        if(it==costs.end() || count<=0 || count>it->second.each-it->second.seen)return count;
        it->second.seen+=count;++removalCalls;
        return static_cast<std::int32_t>(static_cast<std::int64_t>(count)*quantity);
    }
    std::int32_t addition(std::uint32_t id,std::int32_t count) {
        if(id!=selection.output || count<=0 || static_cast<std::uint32_t>(count)>selection.units-outputSeen)return count;
        outputSeen+=count;++outputCalls;return static_cast<std::int32_t>(static_cast<std::int64_t>(count)*quantity);
    }
    float experience(float amount) {
        if(!std::isfinite(amount) || amount<=0 || amount>std::numeric_limits<float>::max()/quantity)return amount;
        ++skillCalls;return amount*static_cast<float>(quantity);
    }
    std::int64_t totalOutput() const {return static_cast<std::int64_t>(selection.units)*quantity;}
    bool accounted() const {
        if(outputSeen!=selection.units)return false;
        for(const auto& [id,c]:costs)if(c.seen!=c.each)return false;
        return true;
    }
};
// Synchronous extent: restored even when a callback exits early or throws.
// Used both to activate a transaction and to suspend it around chained hooks.
template<class T> class Scope {
    T*& slot;T* previous;
public:
    Scope(T*& s,T* value):slot(s),previous(s){slot=value;}
    ~Scope(){slot=previous;}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
