# Experimental direct flail SMP — runtime gate

This is an untested replacement FSMP DLL for a **two-silver-flail** experiment.
It is based on FSMP v3.2.1 for the user-reported Skyrim 1.6.1170 setup.
Confirm successful SKSE/Address Library loading before testing. A successful
compiler result is not a compatibility guarantee.

## First test setup

1. Use an isolated MO2 test profile and a fresh/disposable save. Keep the existing
   working profile and saves for rollback. Existing saves can retain old script
   instances; this prototype does not migrate them.
2. Keep the original flail mod, the supplied Lorerim replacement, IED, and FSMP's
   existing configuration/dependencies. Disable every earlier GildFlail IED
   helper patch in the test profile, including v0.8 and its furniture quest.
   Use the fresh save so no previous helper script instances are retained.
3. Only after the Windows build succeeds, install the resulting `UNTESTED.zip` as
   an override of FSMP's DLL. Make sure that this package wins the file conflict
   for `SKSE/Plugins/hdtSMP64.dll`.
4. Enable `GildFlail_DirectSMP_Lorerim.esp` after GildFlail.esp and any other
   overrides of the silver weapon. Other changes to the silver weapon need
   review in xEdit, as with any winning weapon override.
5. Use two **silver** flails in third person. No non-silver flail should be
   equipped during this experiment, because its original helper can activate
   the legacy-driver guard. First-person and NPC behavior are outside this test.

## Checks to perform

| Case | Required result |
| --- | --- |
| One silver flail in each hand | Both chains move independently while walking/turning |
| Exit a chair, bed and crafting station | Both visible flails keep moving or rebind without a full SMP reset |
| No other SMP armor/hair equipped or nearby | Flails still simulate without an active armor/hair skeleton |
| Draw and sheath | Both chains remain active or rebind without a full SMP reset |
| Unequip left, keep right equipped | Right continues moving; the visible IED copy moves |
| Re-equip left | Two independent moving flails return |
| Repeat with right hand | Symmetric behavior |
| IED keeps an equipped duplicate hidden | Hidden copy cannot take over the visible copy's binding |
| Remove/drop one weapon | Remaining flail still works; no detached-instance simulation |
| Save/reload, change cell | Bindings rebuild without a crash or a retained old model |
| Wear another item using slot 59 | Direct flail physics continues; the prototype never takes the slot |
| Return to main menu and start/load another game | No stale bindings carry over |

The existing XML defines chain motion; this prototype does not add weapon-hit
collision damage or new world/character collision shapes.

## Logs and failure interpretation

Look in the normal `hdtSMP64.log` for `[FlailDirectSMP]` messages:

- `enabled=true`: the INI and companion marker were detected.
- `bound independent instance ... five bones`: an individual subtree was bound.
  Two different instance addresses should appear for two visible flails.
- `legacy armor physics is present`: older helper physics is still active. The
  prototype deliberately suspends itself to avoid double control.
- `five-bone binding failed`: the model/XML did not match this prototype.
- `scene traversal limit reached`: the scene exceeded this prototype's bounds.

There are no on-screen notifications. A binding message alone does not prove
that visible mesh skinning works: inspect both flails moving independently.
Failed bindings are not retried every frame; restart after fixing their cause.

If a crash or regression occurs, restore the original FSMP DLL, disable this
companion, restore the earlier helper patch, and return to the untouched profile
and save. Provide the build log, FSMP log, exact runtime versions, and any crash
log before the next revision. Do not save over a normal playthrough while testing.
