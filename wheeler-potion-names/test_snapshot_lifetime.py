"""Exercise the actual adapter/cache methods with a small fake inventory.

The game is not available here. ASan checks the specific stale-extra-data risk;
engine lookup, equip, rendering, and save/load still require an in-game test.
"""
from pathlib import Path
import os
import subprocess
import sys

root = Path(__file__).resolve().parent
source_root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root/'repo'
source = (source_root/'src/bin/Wheeler/WheelItems/WheelItemAlchemy.cpp').read_text()
helper = source[source.index('\t// Refined retains inventory snapshots'):source.index('\tconstexpr RE::FormID kTrackedI4DiagFormID')]
methods = source[source.index('int WheelItemAlchemy::GetBatchCount'):source.index('bool WheelItemAlchemy::EquipInventoryBatch')]
mock = r'''
#include "bin/Wheeler/WheelItems/AlchemyBatch.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <cstdlib>

namespace RE {
using FormID = std::uint32_t;
struct TESBoundObject {
    virtual ~TESBoundObject() = default;
    template<class T> T* As() { return dynamic_cast<T*>(this); }
};
struct AlchemyItem : TESBoundObject {
    FormID id = 42;
    const char* GetName() { return "Potion"; }
    FormID GetFormID() { return id; }
};
inline AlchemyItem potion;
struct TESForm {
    template<class T> static T* LookupByID(FormID id) { return id == potion.id ? potion.As<T>() : nullptr; }
};
struct ExtraDataList {
    std::string name;
    int count;
    int GetCount() { return count; }
    const char* GetDisplayName(AlchemyItem*) { return name.c_str(); }
};
struct Entry { std::unique_ptr<std::vector<ExtraDataList*>> extraLists; };
struct TESObjectREFR {
    using InventoryItemMap = std::unordered_map<TESBoundObject*, std::pair<int, std::unique_ptr<Entry>>>;
};
}
struct WheelItemAlchemy {
    RE::FormID _formID = 42;
    std::optional<std::string> _inventoryName;
    int GetBatchCount(RE::TESObjectREFR::InventoryItemMap&);
    static void CacheInventoryNames(RE::TESObjectREFR::InventoryItemMap&);
    static void ForgetInventoryNames(RE::TESObjectREFR::InventoryItemMap&);
};
'''
test = r'''
static int checks = 0;
static void require(bool condition) { ++checks; if (!condition) std::abort(); }
static RE::TESObjectREFR::InventoryItemMap inventory(int count, std::vector<RE::ExtraDataList*> extras) {
    RE::TESObjectREFR::InventoryItemMap result;
    auto entry = std::make_unique<RE::Entry>();
    entry->extraLists = std::make_unique<std::vector<RE::ExtraDataList*>>(std::move(extras));
    result.emplace(&RE::potion, std::make_pair(count, std::move(entry)));
    return result;
}
int main() {
    auto boss = std::make_unique<RE::ExtraDataList>(RE::ExtraDataList{"Boss Fight", 3});
    auto travel = std::make_unique<RE::ExtraDataList>(RE::ExtraDataList{"Travel", 2});
    auto cached = inventory(5, {boss.get(), travel.get()});
    auto otherCache = inventory(2, {travel.get()});
    WheelItemAlchemy wheel{42, std::string("Boss Fight")};
    WheelItemAlchemy::CacheInventoryNames(cached);
    WheelItemAlchemy::CacheInventoryNames(otherCache);
    require(wheel.GetBatchCount(cached) == 3);
    require(wheel.GetBatchCount(otherCache) == 0);

    // The original cached entry now contains a freed pointer, exactly the
    // condition created when a batch is consumed before the next UI refresh.
    boss.reset();
    require(wheel.GetBatchCount(cached) == 3);  // Uses owned snapshot strings.
    auto live = inventory(2, {travel.get()});
    require(wheel.GetBatchCount(live) == 0);    // Fresh activation sees depletion.
    const auto batches = DescribeBatches(&RE::potion, live);
    require(AlchemyBatch::Select(batches.stacks, wheel._inventoryName).count == 0);

    cached = inventory(2, {travel.get()});
    WheelItemAlchemy::CacheInventoryNames(cached);
    require(wheel.GetBatchCount(cached) == 0);
    boss = std::make_unique<RE::ExtraDataList>(RE::ExtraDataList{"Boss Fight", 1});
    cached = inventory(3, {boss.get(), travel.get()});
    WheelItemAlchemy::CacheInventoryNames(cached);
    require(wheel.GetBatchCount(cached) == 1);
    require(wheel.GetBatchCount(otherCache) == 0);  // Independent cache owners.
    WheelItemAlchemy::ForgetInventoryNames(cached);
    WheelItemAlchemy::ForgetInventoryNames(otherCache);
    require(cachedBatchNames.empty());

    // The adapter must count implicit unrenamed copies and not double-count
    // repeated list pointers, including while selecting an explicit batch.
    auto mixed = inventory(8, {boss.get(), boss.get(), travel.get()});
    const auto mixedBatches = DescribeBatches(&RE::potion, mixed);
    require(AlchemyBatch::Select(mixedBatches.stacks, std::string("Boss Fight")).count == 1);
    require(AlchemyBatch::Select(mixedBatches.stacks, std::string("Potion")).count == 5);
    require(AlchemyBatch::Select(mixedBatches.stacks, std::nullopt).count == 8);
    std::cout << checks << " actual adapter/cache checks passed under ASan and UBSan\n";
}
'''
test_file = root/'snapshot_lifetime_test.cpp'
test_file.write_text(mock + '\nnamespace {\n' + helper + '\n}\n' + methods + test)
executable = root/'snapshot_lifetime_test'
subprocess.run(['g++', '-std=c++20', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-Wall', '-Wextra', '-Werror', '-I', str(source_root/'src'), str(test_file), '-o', str(executable)], check=True)
# This runtime traces child processes, which LeakSanitizer cannot inspect.
# AddressSanitizer's use-after-free checks and UBSan remain enabled.
test_env = dict(os.environ)
test_env['ASAN_OPTIONS'] = 'detect_leaks=0'
subprocess.run([str(executable)], check=True, env=test_env)
