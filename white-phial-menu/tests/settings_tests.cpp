#include "Settings.h"
#include "Keys.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace phial;
    assert(command(repaired, 1) == "set TWPTE_PhialIsFullyRepaired to 1");
    assert(command(repaired, 0) == "set TWPTE_PhialIsFullyRepaired to 0");
    assert(command(hours, 12.5f) == "set TWPTE_ResetHours to 12.5");
    assert(command(hotkey, 68) == "set TWPTE_HotkeyButton to 68");
    assert(command(hotkey, 181) == "set TWPTE_HotkeyButton to 181");
    assert(!command(count, 1));
    assert(!command(repaired, 2));
    assert(!command(hours, 0));
    assert(!command(hours, -1));
    assert(!command(hours, std::numeric_limits<float>::quiet_NaN()));
    assert(!command(hours, std::numeric_limits<float>::infinity()));
    assert(command(hours, minHours));
    assert(command(hours, maxHours));
    assert(!command(hours, maxHours + 1));
    assert(!command(hotkey, 68.5f));
    assert(!command(hotkey, 282));
    assert(!command(hotkey, 0));
    assert(keyName(181) == "Numpad /");
    assert(keyName(68) == "F10");
    assert(keyName(276) == "Gamepad A");
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        assert(valid(hotkey, static_cast<float>(keys[i].code)));
        for (std::size_t j = i + 1; j < std::size(keys); ++j) assert(keys[i].code != keys[j].code);
    }

    // Installation/refresh does not reset the player's existing settings.
    Draft draft;
    Values original{ 1, 7.25f, 88 };
    draft.receive(1, original);
    assert(draft.request.dirty == 0 && draft.request.desired == original);

    // Changing only the hotkey does not reset enchantment or refill hours.
    draft.edit(hotkey, 181);
    assert(draft.request.dirty == 4);
    Values changedByQuest{ 1, 3.5f, 88 };
    draft.receive(1, changedByQuest);
    assert(draft.request.desired[hours] == 3.5f);
    assert(canApply(draft.request, 1, changedByQuest));

    // A pending edit may not cross a save load or overwrite another writer.
    assert(!canApply(draft.request, 2, changedByQuest));
    Values conflict{ 1, 3.5f, 68 };
    draft.receive(1, conflict);
    assert(draft.request.desired[hotkey] == 181);
    assert(!canApply(draft.request, 1, conflict));
    draft.reset(1, conflict);
    draft.edit(hotkey, 181);
    assert(canApply(draft.request, 1, conflict));

    // Successful readback clears edits. A new save resets the whole draft.
    draft.receive(1, Values{ 1, 3.5f, 181 });
    assert(draft.request.dirty == 0);
    draft.edit(hours, 12);
    draft.receive(2, Values{ 0, 24, 181 });
    assert(draft.request.dirty == 0 && draft.request.desired[hours] == 24);
    draft.edit(hours, 0);
    assert(!draft.validEdits());
    assert(!canApply(draft.request, 2, Values{ 0, 24, 181 }));
    draft.edit(hours, 24);
    assert(draft.request.dirty == 0);
}
