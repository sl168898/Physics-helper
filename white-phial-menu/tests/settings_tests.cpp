#include "Settings.h"
#include "Keys.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace phial;
    assert(valid(repaired, 0) && valid(repaired, 1));
    assert(!valid(repaired, 2) && !valid(count, 1));
    assert(valid(hours, minHours) && valid(hours, maxHours));
    assert(!valid(hours, 0) && !valid(hours, -1) && !valid(hours, maxHours + 1));
    assert(!valid(hours, std::numeric_limits<float>::quiet_NaN()));
    assert(!valid(hours, std::numeric_limits<float>::infinity()));
    assert(valid(hotkey, 47) && valid(hotkey, 181));
    assert(!valid(hotkey, 68.5f) && !valid(hotkey, 282) && !valid(hotkey, 0));
    assert(keyName(47) == "V");
    assert(keyName(181) == "Numpad /");
    assert(keyName(68) == "F10");
    assert(keyName(276) == "Gamepad A");
    for (std::size_t i = 0; i < std::size(keys); ++i) {
        assert(valid(hotkey, static_cast<float>(keys[i].code)));
        for (std::size_t j = i + 1; j < std::size(keys); ++j) assert(keys[i].code != keys[j].code);
    }

    // Regression: the user's V-key change is valid and touches only hotkey.
    Draft crashCase;
    crashCase.receive(1, Values{ 1, 12, 181 });
    crashCase.edit(hotkey, 47);
    assert(crashCase.request.dirty == (1u << hotkey));
    assert(canApply(crashCase.request, 1, crashCase.current));
    crashCase.receive(1, Values{ 1, 12, 47 });
    assert(!crashCase.request.dirty && crashCase.current[hours] == 12);

    // User's pre-existing X binding can move through V, N, top-row 5 and X.
    // Each request preserves the other two settings and sharing preference.
    Draft keySequence;
    Values keyValues{ 1, 12, 45 };
    keySequence.receive(8, keyValues, true);
    for (const float code : { 47.0f, 49.0f, 6.0f, 45.0f }) {
        keySequence.edit(hotkey, code);
        assert(canApply(keySequence.request, 8, keyValues, true));
        assert(keySequence.request.dirty == (1u << hotkey));
        keyValues[hotkey] = code;
        keySequence.receive(8, keyValues, true);
        assert(!keySequence.request.dirty && keySequence.request.remember);
        assert(keyValues[repaired] == 1 && keyValues[hours] == 12);
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

    // Sharing can be enabled/disabled without editing any individual setting.
    draft.request.remember = true;
    assert(draft.validEdits());
    assert(canApply(draft.request, 2, Values{ 0, 24, 181 }));
    assert(!canApply(draft.request, 2, Values{ 0, 24, 181 }, true));
    draft.receive(2, Values{ 0, 24, 181 }, true);
    assert(!draft.validEdits());
    assert(draft.request.remember && draft.request.expectedRemember);
    draft.request.remember = false;
    assert(draft.validEdits());
    assert(canApply(draft.request, 2, Values{ 0, 24, 181 }, true));
    draft.receive(2, Values{ 0, 24, 181 }, false);
    assert(!draft.validEdits());

    // A failed disk save can be retried even when game values already match.
    assert(!canApply(draft.request, 2, draft.current, false));
    assert(canApply(draft.request, 2, draft.current, false, true));
    draft.request.remember = true;
    draft.receive(3, Values{ 1, 6, 68 }, false);
    assert(!draft.request.remember);  // Cancel unapplied toggle on a save switch.

    // Enabling sharing validates ALL values, including previously unedited ones.
    draft.reset(4, Values{ 1, -5, 68 });
    draft.request.remember = true;
    assert(!draft.validEdits());
    assert(!canApply(draft.request, 4, draft.current));

    // The fourth field participates in dirty masks and save-load isolation.
    assert(valid(autoDecant, 0) && valid(autoDecant, 1) && !valid(autoDecant, 2));
    draft.reset(5, Values{ 1, 24, 45, 0 });
    draft.edit(autoDecant, 1);
    assert(draft.request.dirty == 8 && draft.validEdits());
    assert(canApply(draft.request, 5, draft.current));
    assert(!canApply(draft.request, 6, draft.current));
    draft.receive(5, Values{ 1, 24, 45, 1 });
    assert(!draft.request.dirty && draft.request.desired[autoDecant] == 1);
    draft.edit(autoDecant, 0.5f);
    assert(!draft.validEdits());
}
