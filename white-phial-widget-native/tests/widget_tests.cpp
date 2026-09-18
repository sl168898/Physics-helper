#include "Config.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>

int main()
{
    using namespace widget;
    // All reachable combinations, including duplicate phials and no phial.
    for (int bits = 0; bits < 8; ++bits) {
        const State expected[] = { State::absent, State::empty, State::potion, State::potion,
            State::poison, State::poison, State::potion, State::potion };
        assert(classify(bits & 1, bits & 2, bits & 4) == expected[bits]);
    }
    Settings cfg;
    Snapshot view{true, true, false, false, State::potion};
    assert(visible(cfg, view, false));
    view.state = State::absent;
    assert(!visible(cfg, view, false));
    cfg.hideAbsent = false;
    assert(visible(cfg, view, false));
    view.inMenu = true;
    assert(!visible(cfg, view, false));
    assert(visible(cfg, view, true));
    cfg.hideInMenus = false;
    assert(visible(cfg, view, false));
    cfg.enabled = false;
    assert(!visible(cfg, view, false));
    assert(visible(cfg, view, true));
    view.blocked = true;
    assert(!visible(cfg, view, true));
    view.blocked = false; view.active = false;
    assert(!visible(cfg, view, true));
    view.active = true; view.ready = false; cfg.enabled = true;
    assert(!visible(cfg, view, false));

    // Save transition with a queued task: no status from the old save can leak.
    Session s;
    assert(s.request() == 0);
    s.reset(true);
    const auto oldTicket = s.request();
    assert(oldTicket != 0 && s.request() == 0);
    s.reset(false);
    assert(!s.finish(oldTicket, {true, true, false, false, State::potion}));
    s.reset(true);
    const auto newTicket = s.request();
    assert(!s.finish(oldTicket, {true, true, false, false, State::potion}));
    assert(s.pending);
    assert(s.finish(newTicket, {true, true, false, false, State::empty}));
    assert(s.view.state == State::empty && !s.pending);
    assert(!s.finish(newTicket, {true, true, false, false, State::potion}));

    // The same settings survive save switches and process restarts through INI.
    Settings chosen{false, false, false, false, 23.4f, 65.2f, 170.0f, 45.0f};
    auto parsed = parseConfig(serializeConfig(chosen));
    assert(parsed.error.empty() && parsed.settings == chosen);
    assert(parseConfig("# comment\n[Widget]\nX = 20\r\nY=30\n").error.empty());
    for (auto bad : {"X=nan", "Y=inf", "X=-1", "X=101", "Scale=0", "Opacity=101",
        "Enabled=yes", "ShowLabel=2", "X=50junk", "X=1\nX=2", "Y="})
        assert(!parseConfig(bad).error.empty());
    const auto dir = std::filesystem::temp_directory_path() /
        ("WhitePhialWidgetTests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto path = dir / "WhitePhialWidget.ini";
    assert(loadConfig(path).error.empty());
    assert(saveConfig(path, chosen).empty());
    assert(loadConfig(path).settings == chosen);
    auto invalid = chosen; invalid.x = std::numeric_limits<float>::quiet_NaN();
    assert(!saveConfig(path, invalid).empty());
    assert(loadConfig(path).settings == chosen); // Bad writes preserve previous preferences.
    chosen.x = 50;
    assert(saveConfig(path, chosen).empty());
    assert(loadConfig(path).settings == chosen);
    std::filesystem::remove_all(dir);

    // Position remains on screen at different resolutions and large scale.
    Settings corner; corner.x = corner.y = 100; corner.scale = 250;
    for (auto dimensions : {std::pair{1280.0f, 720.0f}, {3440.0f, 1440.0f}, {3840.0f, 2160.0f}}) {
        auto pos = layout(corner, dimensions.first, dimensions.second, 350.0f);
        assert(pos.x >= 0 && pos.y >= 0);
        assert(pos.x + 62*pos.unit + 350 <= dimensions.first);
        assert(pos.y + 56*pos.unit <= dimensions.second);
    }
    std::cout << "Passed inventory states, visibility, save isolation, persistent settings, and display layout.\n";
}
