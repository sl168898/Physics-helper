# White Phial Status Widget — SKSE 2.0

A native, read-only HUD indicator for **The White Phial - Tweaks and Enhancements**.

- Grey phial: **Empty**.
- Gold phial: **Filled - Potion** (all six preset potions and the custom potion).
- Green phial: **Filled - Poison**.
- Hidden by default when you are not carrying the phial or while a menu is open.

Inventory is checked on the game thread about every 0.2 seconds. Drinking,
refilling, manual decanting, daily decanting and transferring the phial therefore
update the same indicator. The broken quest phial and the original mod's internal
dummy item do not count. If multiple phials are present, a filled potion takes
priority, then filled poison, then empty.

## Requirements

- Skyrim Special Edition **1.6.1170**, SKSE64 for that runtime and Address Library
  for SKSE Plugins (Anniversary Edition).
- **SKSE Menu Framework 3.x**, with its native HUD and texture API.
- **The White Phial - Tweaks and Enhancements.esp**, enabled.

This widget has no ESP/ESL, quests or Papyrus scripts. SkyUI MCM and iWant Widgets
are not required by the native widget. Keep any shared frameworks your other mods use.
This DLL deliberately rejects runtimes other than 1.6.1170.

## Install in Mod Organizer 2

1. If the earlier **White Phial Status Widget v1.0** is installed, turn off its
   widget in its MCM, save and exit the game. Keep a backup of that save.
2. Disable that old widget package, including **White Phial - Status Widget.esp**.
   Install this ZIP as a new MO2 mod and enable it. Do not keep both widgets enabled.
3. Keep the original White Phial mod, **White Phial Menu**, **White Phial - Decanting**,
   and your trait mod enabled. This package does not replace those.
4. Launch the game through SKSE in MO2. No new right-pane plugin entry is expected.
5. Open SKSE Menu Framework and choose **White Phial → Widget**.

Removing the old scripted widget may produce the usual missing-plugin notice on a
save that used it. The new DLL does not require a new game. If you never installed
v1.0, skip steps 1–2's removal instructions and simply install this ZIP.

## Controls and settings

Enable/disable; show/hide text; hide when absent; hide in menus; horizontal and
vertical position; size; opacity. The three Preview buttons show a sample for ten
seconds without changing anything in your inventory. Load a game before previewing.

Changes apply immediately. Click **Save settings** to persist them across saves and
game restarts. **Restore saved** reverts unsaved edits; **Reset defaults** restores
the initial layout. Widget preferences from the old MCM do not migrate automatically.

Saved preferences are in `Data/SKSE/Plugins/WhitePhialWidget.ini` as seen by the game.
With MO2, a newly created file normally appears under **Overwrite → SKSE → Plugins**
(or your configured output mod). Keep it enabled to retain preferences. An update
does not ship an INI that could overwrite them. Characters using that same MO2
configuration share these display settings.

The widget never edits refill timing, hotkeys, phial effects, decant settings, quest
stages, inventory or save data. It reports readiness from actual inventory; it is
not a refill countdown. It respects the game's hidden HUD and loading/title screens.
Support for every third-party HUD auto-hide system has not been tested.

## Quick in-game check

Carry an empty phial, then let it refill: grey should become gold or green.
Drink or decant it: it should return to grey. Transfer the phial to storage: it
should disappear. Try a potion and poison, then save/reload and verify your widget
preferences persist after clicking Save settings. Leave preview mode to expire
before checking the real inventory state.

If the menu page is missing, check `WhitePhialWidget.log` in SKSE's log folder,
normally `Documents/My Games/Skyrim Special Edition/SKSE` (Windows may redirect
Documents to OneDrive). The log reports missing framework exports or unresolved
phial forms. If icons are missing but text appears, check the three PNG files under
`SKSE/Plugins/WhitePhialWidget` in MO2's Data view.

## Build and validation

Source: https://github.com/sl168898/Physics-helper/tree/codex/phial-widget-native/white-phial-widget-native

Run `tools/build_windows.ps1` with Visual Studio 2022 C++ tools, Windows SDK, CMake,
Git and PowerShell 7. Dependencies are pinned in that script. The Windows workflow
builds the x64 DLL, runs tests for state selection, hidden states, save isolation,
configuration persistence and screen positioning, and writes `BuildInfo.json`.
**Those checks are not an in-game test.** Skyrim was not available in the build environment.

The three icon assets were authored for this widget. No assets or source from the
two suggested Nexus widget mods are redistributed. Native drawing uses the SKSE
Menu Framework API; CommonLibSSE-NG supplies the game interfaces. Their licenses are
included. Widget code and artwork are MIT licensed.
