#include "DescriptionText.h"
#include "PublicAPI.h"
#include <cassert>
#include <iostream>
#include <map>
int main() {
    std::map<std::string, std::string> globals{{"chance", "5"}, {"duration", "30"}};
    auto resolve = [&](std::string_view id) -> std::optional<std::string> {
        auto it = globals.find(esd::lowerASCII(id));
        return it == globals.end() ? std::nullopt : std::optional{it->second};
    };
    auto format = [&](std::string_view raw, bool plain = false) { return esd::formatDescription(raw, plain, resolve); };
    // Same two marker values as the user's Inebriation screenshot/source record.
    assert(format("Absorbs <15> stamina; maximum <60> points.") == "Absorbs 15 stamina; maximum 60 points.");
    assert(format("Absorbs <15> stamina; maximum <60> points.", true) == "Absorbs 15 stamina; maximum 60 points.");
    assert(format("<-25>% <+10> <0.5> <1,000> <0>") == "-25% +10 0.5 1,000 0");
    assert(format("<font color='#ffffff'><15></font> points.") == "<font color='#ffffff'>15</font> points.");
    assert(format("<font color='#ffffff'><15></font><br/>points &amp; bonuses.", true) == "15\npoints & bonuses.");
    assert(format("<b>5</b>% for <Global=duration> seconds.", true) == "5% for 30 seconds.");
    assert(format("<Global=CHANCE>% chance") == "5% chance");
    globals["chance"] = "12.5";
    assert(format("<Global=CHANCE>% chance") == "12.5% chance"); // Live global changes, no frozen saved value.
    assert(!format("<Global=Missing>"));
    assert(!format("<mag> damage")); // Never fabricate an unknown effect value.
    assert(format("unchanged plain text") == "unchanged plain text");
    assert(format("Damage <15") == "Damage <15");
    assert(format("耐力 <15>，上限 <60>。", true) == "耐力 15，上限 60。");
    const auto text = *format("<15>/<60>", true);
    assert(esd::api::copyText(text, nullptr, 0) == 6);
    char exact[6]{};
    assert(esd::api::copyText(text, exact, sizeof(exact)) == 6 && std::string(exact) == "15/60");
    char small[3]{'x', 'x', 'x'};
    assert(esd::api::copyText(text, small, sizeof(small)) == 6 && small[0] == '\0' && small[1] == 'x');
    assert(esd::api::copyText("", exact, sizeof(exact)) == 0 && exact[0] == '\0');
    std::cout << "PASS: screenshot numbers, numeric/HTML separation, live globals, UTF-8, plain Wheeler text and C API buffers\n";
}
