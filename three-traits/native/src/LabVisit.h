#pragma once

namespace traits {
// One grant per workbench visit, even when both the menu and furniture send exits.
struct LabVisit {
    bool armed = false;
    void enter(bool isAlchemy) { armed = isAlchemy; }
    void confirmAlchemyMenu() { armed = true; }
    void cancel() { armed = false; }
    bool finish() {
        const bool grant = armed;
        armed = false;
        return grant;
    }
};
}
