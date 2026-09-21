# White Phial - Decanting 2.1.0 beta

Adds a searchable **White Phial / Blacklist** page to SKSE Menu Framework 3.x.
Open that page, search a potion name or source plugin, and click Add. The top
list lets you remove entries. Use Refresh after crafting, renaming, changing
inventory or selecting a different phial liquid. A shortcut blocks/unblocks
current contents, including when the phial is empty.

Blocked liquids cannot be assigned, refilled or decanted, including the daily
automatic decant option. A rejected assignment returns the sample. An already
full phial stays full and can still be used once; it then remains empty.
Removing the current liquid from the blacklist restarts the normal refill
countdown if the tracked phial is empty. No instant free refill is granted.

Search includes loaded non-food potions/poisons and custom liquids in your
inventory or protected bank. Static entries use source plugin + local record
ID. Custom entries match effect identities, magnitudes, areas, durations and
potion/poison type. Names, load-order indices and protected slot numbers are
not part of the custom identity. Renaming or decanting the same recipe cannot
bypass its rule; a different strength is a different recipe. This restriction
does not remove existing bottles or affect ordinary alchemy or drinking.

Rules save automatically to `Data/SKSE/Plugins/WhitePhialBlacklist.cfg`, shared
by saves seeing that file. In MO2 the generated file will normally appear in
Overwrite unless a configured output mod receives it. Keep it when updating;
no default configuration ships in the archive. A separate MO2 profile needs
its own file visibility if you want a different blacklist. This setting is
independent of the menu's Remember option. Unavailable mod entries remain
removable. Invalid configuration stops duplication and reports an error;
failed writes retain the old rules. Up to 4096 rules, 16 MiB total.

Install the complete archive over the previous decant addon and restart
Skyrim. Let its DLL and all nine scripts win conflicts, including
`MS12PostQuestScript.pex`, `MS12WhitePhialScript.pex` and the assignment actor
script. The two original refill-script overrides are new in 2.1.0. Keep the
original phial mod, SKSE Menu Framework 3.x, and the existing White Phial Menu /
Widget enabled. This DLL supplies the additional page; it does not replace
their controls. Other mods replacing the same refill scripts need merging.

This retains the 2.0.3 early co-save lookup fix and existing potion protection.
The ESP, stable slot IDs and bank payload format are unchanged. Existing
protected definitions remain in their assigned slots. No new background
inventory monitoring is added; search snapshots run on opening/refresh.

Built for **Skyrim Steam 1.6.1170**, SKSE64 2.2.6 and AE Address Library.
Build/tests verify native identity/persistence and compiled script guard paths;
Skyrim is not available in the build environment. Test using a separate save:
block an ordinary and a renamed custom potion, attempt assignment, refill and
both decant modes, then remove the rule, save, quit and reload. Check the list
persists and existing protected bottles still appear in inventory and Wheeler.
Keep the original save and matching .skse pair until this succeeds.

## Protected custom liquids (2.0 beta behavior)

This overhaul gives newly decanted player-created potions and poisons stable,
plugin-backed definitions. It is a preventive beta, not a repair for missing
forms in Save29 and not a confirmed fix for the still-unidentified first deleter.
The Windows build and automated checks can run here; Skyrim cannot, so the
save/load, drinking, poison and Wheeler checks below still need an in-game test.

## Install in MO2

1. Back up a working save **and its matching .skse file**. Use a separate test
   profile/save branch. Close the assignment menu and let the phial finish
   changing before making that backup. Do not overwrite your pre-update saves.
2. Replace the old White Phial - Decanting addon with this complete archive.
   Keep only one version of the addon enabled. Keep the original **The White
   Phial - Tweaks and Enhancements** and its requirements enabled.
3. Let this version win file conflicts for `WhitePhialNames.dll`, the WPD
   scripts, `TWPTE_WhitePhialActorScript.pex`, `MS12PostQuestScript.pex` and
   `MS12WhitePhialScript.pex`. These are intentional overrides; the actor is an
   intentional override of the supplied original mod's selection script.
   Do not install only the DLL: the updated ESP and scripts are required.
4. Keep `White Phial - Decanting.esp` enabled in its existing load-order
   position after the original phial mod. Do not rename or compact it. All ten
   existing record IDs and the master list are preserved; it remains ESL flagged.
5. Start through SKSE. Let gameplay run briefly outside menus. A valid current
   custom selection is protected once on loading, and its phial effects are
   restored after the native bank has finished validation. Reassign a renamed sample
   once if you are upgrading from 1.1 and want its custom name captured.
6. Let the phial refill, decant, and add the **new bottle** to Wheeler. Old
   wheel entries for the original crafted item do not automatically become
   entries for the protected item; remove those entries and add the new bottle.

Requires **Skyrim Steam 1.6.1170**, SKSE64 2.2.6 and the AE Address Library.
The DLL deliberately refuses other runtimes. Keep the existing Rename Potions /
Wheeler / Wheeler Refined compatibility patch if you use those mods. Their DLLs
are not included. White Phial Menu and Widget can remain enabled.

## Protection provided

- Snapshot the selected custom liquid while its sample still exists, before
  consumption. Protect the saved recipe, not just the next bottled dose.
- Preserve each effect's base magic effect, magnitude, duration, area and cost;
  potion/poison flags, value settings, weight, captured name, bottle model,
  icon paths, keywords, equip slot, bounds, addiction and sound references.
- Copy effects into owned storage, without sharing the original potion's
  Effect pointers or depending on its native Created Objects reference count.
- Use one stable slot per distinct definition, including its captured name.
  Never recycle a used slot within that save. Changing contents cannot change
  older bottles, including bottles left in unloaded containers.
- Save the bank in the matching SKSE co-save. Read and resolve it at SKSE's
  **PreLoadGame** event, before Skyrim restores inventory and active effects.
  The normal SKSE load callback verifies the same bytes without replacing
  pointers that restored active effects may already be using.
- Address static effect/keyword/sound dependencies by plugin filename and
  local ID. Address named runtime keywords by unique EditorID, so changes to
  their generated numeric IDs do not lose the tags on reload.
- Cross-check a checksum stored as two exact 16-bit global values in the ESS
  against the co-save's bank. Missing/mismatched/corrupt bank data disables
  assignment and decanting and displays an error. Keep the original pair and
  inspect the logged path/error; this does not reconstruct deleted co-save data.
- Refuse unsupported dynamic liquid definitions or an exhausted pool before
  consuming the sample/full phial. There is no fallback to a temporary bottle.
- Keep the existing decant power, refill timing, poison-use counter handling,
  six preset bottled essences and optional 8 AM schedule. Ordinary game/mod
  potions with stable base forms continue to use their original records.

There are **1,792 distinct protected custom-liquid definitions per save**.
Repeated doses of a definition use its existing slot and do not consume new
slots. This limit avoids growing or reusing native FF created-object forms.
The new safeguards run on assignment, decanting and save/load events. They
add no inventory polling or permanent sample actor/container. The existing
1.1/1.2 power/daily-schedule timer remains unchanged.

## Existing bottles and damaged saves

Only bottles produced through the protected path use stable records. This
update does **not** rewrite previously decanted bottles in inventory,
containers, followers or Wheeler. Those retain their old FF form IDs and can
still be vulnerable. Do not treat installing this as protection for an old
stockpile. Rebuild that stock using newly decanted protected doses from a
working save and update its wheel entries.

Save28 already contains the suspected lifetime problem; Save29 contains
missing definitions. This beta does not clean either save or make dangling
inventory/Wheeler references safe. Use an earlier working save for the beta
or a separately repaired copy once a repair has been validated. The diagnostic
remains useful for identifying and correcting the original deletion mechanism.

Keep this ESP, its DLL and scripts installed in saves using protected bottles.
Removing the mod, losing the matching co-save, or removing mods that define
its magic effects is not supported. A placeholder with "definition unavailable"
in its name indicates missing storage, not a usable substitute potion.

## First in-game verification

1. Select a newly brewed potion with recognisable numbers and a custom name.
   Decant two doses; confirm name, effects, strength, duration, weight and value.
2. Put one in a safe container and one on Wheeler. Assign a different liquid
   to the phial and confirm both older doses stay unchanged.
3. Save, fully quit Skyrim, restart and load. Check both bottles and Wheeler.
4. Drink a protected duration potion, save while its effects are active, then
   fully restart and load. Confirm the effects and remaining bottles behave.
5. Repeat with a custom poison: apply it to a weapon and confirm the effect and
   ordinary phial refill behaviour. Check daily decanting if you use it.
6. Only after these checks, play on this separate save branch. If a failure
   occurs, preserve the paired ESS/SKSE files and `WhitePhialNames.log` before
   another launch truncates the log. The log is in the usual Skyrim SKSE log
   folder under Documents/My Games/Skyrim Special Edition/SKSE.

## Implementation and build

Source is included. Dependencies are pinned in `tools/build_windows.ps1`.
`tools/build_plugin.py` preserves original records and appends the slot bank.
Compiled Papyrus payloads are packaged with their source hashes and build
manifest. Compile-only API stubs are not installed.

Dynamic liquids with conditional effect entries, alternate texture swaps,
model add-ons, destructible data, or unsupported temporary dependencies
are refused rather than silently simplified. Uniquely named runtime keywords
registered in the keyword array are supported. Standard player-brewed potions
and poisons use the supported effect structure. This is not a generic cloning
framework for arbitrary scripted alchemy items.

The native code retains published effect allocations through the process
lifetime because engine active effects borrow their pointers during save
switches. Identical definitions share this session cache, so repeatedly
loading the same save does not allocate another copy each time. There is no
per-frame scan or reference-count interception in this helper.

The co-save framing and load-event sequence follow SKSE 2.2.6 source:
https://github.com/ianpatt/skse64/tree/9398d04592a7eb9d754f2997701116df1022f1b4
Original mod: https://www.nexusmods.com/skyrimspecialedition/mods/73532

KID creates named keywords during DataLoaded and inserts them into the keyword
array, so the runtime lookup uses that array rather than only the general
EditorID map. Source inspected for this behavior:
https://github.com/powerof3/Keyword-Item-Distributor/blob/99adaaa1d8cefe320524802b57e91723f66b1c7c/src/Data/FormData.cpp
https://github.com/powerof3/Keyword-Item-Distributor/blob/99adaaa1d8cefe320524802b57e91723f66b1c7c/src/Cache.cpp
