# White Phial - Decanting 2.0.3 beta

Fixes the early co-save filename lookup on reload. SKSE's PreLoadGame event
can supply a name ending in `.ess`; 2.0.2 appended `.skse` without removing
that suffix and could therefore look for `Save30.ess.skse`. The new reader
uses SKSE 2.2.6's extension handling and its Steam save-folder construction,
including `sLocalSavePath:General`. It no longer infers the game save folder
from CommonLib's log-folder detection.

The supplied Save30 pair was checked read-only: it contains one protected
bottle and one intact recipe, whose fingerprint matches the ESS exactly.
The before-restart log confirms successful protection and decanting; the
after-restart log shows the early loader missed a record SKSE later read.
The old log did not record its filename, so it cannot prove which incorrect
path was opened in that session. This update logs the incoming name, selected
path, byte count, fingerprint and normal-callback verification explicitly.

Install this complete archive over 2.0.2 in MO2, let its files win conflicts,
and restart Skyrim. Load the original Save30 **with its matching .skse**.
Do not clean the save, reset quests, reassign the phial or recreate that
bottle. Check the existing named bottle in inventory and Wheeler. Save to a
new slot, quit and reload that new save, then check again. Keep Save30's
original pair until this in-game check succeeds.

The ESP, Papyrus scripts, slot IDs and bank format are byte-identical to
2.0.2. The native plugin reports 2.0.3; the unchanged decant-script trace
still says 2.0.2. Effects are still restored before engine save loading, and
the normal callback verifies the same bytes. No late reconstruction or
fallback to another save is used. Missing and unreadable files are now
reported distinctly, and the first specific loader error is retained.

The 2.0.2 fixes are included: runtime keywords are preserved by unique
EditorID, and the compiled caller uses the original refill script's verified
`SetForRefill(Actor)` signature. The original refill scripts are not replaced.

Existing v1 banks keep their exact bytes and saved fingerprints. Banks that
contain named runtime keywords use payload v2, while retaining all old slot
assignments. Once such a bank has been saved, keep 2.0.2 or later installed;
2.0/2.0.1 cannot read the new payload. A missing or ambiguous named keyword
still stops loading/protection, rather than silently removing a dependency.
The ESP and existing record IDs are unchanged.

Detailed decant/refill logging remains in
`Documents/My Games/Skyrim Special Edition/SKSE/WhitePhialNames.log`.
Papyrus logging is not required. Windows compilation and automated tests do
not replace the in-game assignment, decant and save/reload test above.

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
   scripts, and `TWPTE_WhitePhialActorScript.pex`. That last script is an
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
