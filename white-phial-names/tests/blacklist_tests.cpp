#include "BlacklistCore.h"
#include <cassert>
#include <iostream>
using namespace phial;
int main() {
    storage::Effect e{{"Skyrim.esm",0x39E51},18,99,0,120};
    auto key=blacklist::recipeKey(false,{e});
    auto changed=e; changed.cost=0; changed.base.file="skyrim.ESM";
    assert(key==blacklist::recipeKey(false,{changed}));
    changed.magnitude=19; assert(key!=blacklist::recipeKey(false,{changed}));
    changed=e; changed.duration=121; assert(key!=blacklist::recipeKey(false,{changed}));
    assert(key!=blacklist::recipeKey(true,{e}));
    assert(blacklist::recipeKey(false,{e,changed})==blacklist::recipeKey(false,{changed,e}));
    auto staticID=blacklist::staticKey({"PotionMod.esp",0x812});
    assert(staticID==blacklist::staticKey({"potionmod.ESP",0x812}));
    assert(staticID!=blacklist::staticKey({"AnotherMod.esp",0x812}));
    // Display names, protected slot assignment and runtime FF/load-order IDs
    // never participate in recipe identity.
    blacklist::Rules rules{{key,"Renamed \"Potion\"\nSecond line"},{staticID,"Static potion"}};
    assert(blacklist::decode(blacklist::encode(rules))==rules);
    for (auto bad : {"", "WhitePhialBlacklist 2\n", "WhitePhialBlacklist 1\n\"R:zz\" \"X\"", "WhitePhialBlacklist 1\n\"R:00\""}) {
        bool rejected=false; try { (void)blacklist::decode(bad); } catch(...) { rejected=true; } assert(rejected);
    }
    auto dir=std::filesystem::temp_directory_path()/"white-phial-blacklist-tests";
    std::filesystem::remove_all(dir); auto path=dir/"rules.cfg";
    assert(blacklist::load(path).empty()); blacklist::save(path,rules); assert(blacklist::load(path)==rules);
    auto original=rules; rules.erase(key); blacklist::save(path,rules); assert(!blacklist::load(path).contains(key));
    auto invalid=rules; invalid["invalid"]="invalid";
    bool rejected=false; try { blacklist::save(path,invalid); } catch(...) { rejected=true; }
    assert(rejected && blacklist::load(path)==rules);
    // Failed replacement must preserve the previous file.
    std::filesystem::create_directory(path.string()+".tmp"); rejected=false;
    try { blacklist::save(path,original); } catch(...) { rejected=true; }
    assert(rejected && blacklist::load(path)==rules);
    std::filesystem::remove_all(dir);
    std::cout << "Blacklist recipe/static identity, round trip, malformed input and failed-write preservation passed\n";
}
