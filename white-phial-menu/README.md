# White Phial Menu

An SKSE Menu Framework settings page for **White Phial - Tweaks and Enhancements** by AndrealletiusVIII. Built for **Skyrim SE/AE 1.6.1170 on Windows**.

Version **1.1.2**, package **v4**, refreshes the original script's hotkey registration when settings change and after loading, while retaining the v3 crash fix and shared settings.

The reported crash occurred while the old adapter compiled a temporary console script. This version removes that path from both manual Apply and automatic restoration. It writes the three resolved globals directly on the game thread. Existing shared INI settings remain compatible.

## Hotkey registration fix

Inspection of the supplied original ESP, source and compiled PEX showed that `TWPTPE_Hotkey_Script` registers its key only in OnEffectStart. Its OnKeyDown also checks the current global. Changing that global alone leaves the old key registered and the new key unregistered. This affects a save whose X key was configured before installing this menu too.

The adapter now finds the player's active instance of the original `TWTPE_Hotkey_Effect` (original plugin local FormID 0xD4D), finds its existing `TWPTPE_Hotkey_Script` VM object, and calls the inherited SKSE `RegisterForKey` method with the selected code. It does this after Apply and on ready load/refresh, even with sharing off. Calls are deduplicated by session, effect handle and key; opening the settings page retries when the effect or script was not ready at load. When the original effect starts later, its own OnEffectStart registers the current key as usual.

Old registrations are retained on that same script to avoid an asynchronous unregister/register race. The original OnKeyDown checks the global, so only the currently selected key can use the phial. Other mods' registrations are untouched. No spell is restarted, no potion is consumed by Apply, and no original script is replaced. The menu/log distinguish a written global, a queued registration call, and a completed call. Completion does not prove an in-game keypress has been tested.

## Upgrade and enable shared settings

1. Close Skyrim, replace the previous White Phial Menu mod with this package in MO2, and enable it. Keep only the new `WhitePhialMenu.dll` active.
2. Load a save, open **White Phial > Settings**, and choose your enchantment status, refill time and hotkey.
3. Check **Remember settings across saves**, then click **Apply changes**. The status should say **Applied and remembered for all saves and new games**.
4. Those three choices are reapplied when loading a different/older save or starting a new game, including after exiting Skyrim and restarting. You do not need to open the settings panel on each load.

Sharing includes **Fully re-enchanted**, so that flag is also applied to other characters. It still does not grant the phial or complete the original quest. The option is off until you enable it and apply, preserving existing settings on installation.

Uncheck the option and click Apply to stop automatic application on future loads. Turning it off does not undo values already applied to the currently loaded game; change them in the menu as needed and save.

## Install with Mod Organizer 2

1. Keep the original [White Phial mod](https://www.nexusmods.com/skyrimspecialedition/mods/73532) and its requirements installed. The hotkey global requires version 2.1 or later; the published configuration for 2.3.1 was used for this adapter.
2. Install [SKSE Menu Framework 3.x](https://www.nexusmods.com/skyrimspecialedition/mods/120352) with its listed requirements. Use a framework build compatible with your Skyrim runtime. This adapter uses the stable exported widgets and checks each export at startup.
3. Have SKSE64 and the AE Address Library for **1.6.1170** enabled, as with the earlier native patches.
4. Install this ZIP using MO2's **Install a new mod from an archive** and enable it in the left pane. It provides `SKSE/Plugins/WhitePhialMenu.dll`. There is no new ESP to order in the right pane.
5. Launch Skyrim through SKSE in MO2 and load a save. Open the Mod Control Panel using your configured framework shortcut (**F1 by default**), then choose **White Phial > Settings**.

## Controls

- **Fully re-enchanted** changes the original mod's full-repair flag. It does not give you the item or complete/rewind the original quest. The refill-time field is separate; checking this option does not impose a new refill time.
- **Refill time (game hours)** edits the original refill-delay setting. Presets: 6, 12, 24 and 48 hours. Custom values: 0.1 through 8760 hours. An already running refill follows the original mod's scripts; this adapter does not cancel or restart its timer.
- **Use phial hotkey** offers readable keyboard, mouse and gamepad names using SKSE's key codes. It configures the original hotkey; potion/poison restrictions and input behavior remain controlled by the original mod. The default is **Numpad /** (181).
- **Apply changes** writes only settings you edited and reads them back. **Discard edits** reloads the currently observed values.
- **Remember settings across saves** stores all three current choices outside the save file and reapplies them after loading. Its checked/unchecked state is also stored. Press Apply to commit a checkbox change even if no other field was edited.

With sharing off, save the game after applying changes; values belong to that save as before. With sharing on, Apply writes the shared configuration immediately, independently of saving the game. This adapter uses ordinary game globals and has no custom save serialization or Papyrus replacement. Removing the DLL removes the menu and automatic application; values already saved retain their normal game behavior.

## Shared configuration and MO2

The menu generates `Data/SKSE/Plugins/WhitePhialMenu.ini` after you enable sharing and press Apply. Through MO2, a newly created file normally appears under **Overwrite/SKSE/Plugins** unless you configured another output mod. An existing file may be updated in the mod that supplies it. Keep the configuration enabled/visible in the MO2 setup whose saves should share these settings. MO2 profiles that expose the same configuration share the choices.

The ZIP does **not** include an INI, so installing an update cannot replace your saved choices. You can move the generated INI from Overwrite into a dedicated enabled configuration mod if you prefer. The game needs write access to that configuration to remember subsequent changes.

The file records `RememberAcrossSaves`, `FullyReenchanted`, `RefillHours` and `Hotkey` in a `[WhitePhial]` section. Missing configuration means sharing is off. A malformed or incomplete enabled profile is rejected and reported in the menu/log. A failed write leaves the previous file intact; the menu reports that the current game changed but the choices could not be remembered, and Apply can retry.

Automatic application happens once after the loaded game is available. Loading-menu and main-menu close events complete deferred work when loading was still in progress. The menu does not continuously force the values during play; scripts/quest progress can still change them, and the remembered choices are reapplied at the next load. Load-time reads never replace the stored profile with the incoming save's values.

## Implementation

The adapter locates unique `TESGlobal` records by their retained EditorIDs and checks their originating plugin. It does not depend on load-order FormIDs or an EditorID-retention extension. All reads and changes run through SKSE game-thread tasks; the render callback uses a locked snapshot. Stale edits are rejected if another script changed that field, and pending work is invalidated on save loads. File writes use a temporary file followed by replacement and report failures. See [Microsoft's MoveFileEx documentation](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw) for the Windows replacement operation.

Each changed field is validated, then written directly to its resolved `TESGlobal::value` and read back. Deleted or constant globals are rejected. No temporary Script is created and no console command is compiled or executed. Globals use Skyrim's Global Variables save table; no speculative TESForm change flag is added. ConsoleUtil is not required. See [CommonLib's TESGlobal layout](https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/T/TESGlobal.h) and [xEdit's save structure](https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.6/Core/wbDefinitionsTES5Saves.pas).

| Menu control | Original global |
| --- | --- |
| Fully re-enchanted | `TWPTE_PhialIsFullyRepaired` |
| Refill time | `TWPTE_ResetHours` |
| Use phial hotkey | `TWPTE_HotkeyButton` |

No original mod assets, ESP edits, new inventory items, quests, or Papyrus scripts are included.

## Validation and first in-game check

The package build compiles the DLL on Windows and runs native tests for the reported V-key (47) edit, invalid numeric values, partial edits, conflicting changes and save isolation. Persistence tests cover disk round trips, replacing an existing profile, restart/disable behavior, regional decimal formatting, corrupt/incomplete files and preserving the old file on a failed write. **It has not been tested in a running Skyrim instance or through MO2's virtual filesystem.** BuildInfo.json records the source revision, dependency revisions and DLL hash.

First use a filled potion phial (not poison). Change X (45) to V (47), press Apply, and close the settings menu. Confirm the new key consumes the filled phial and the previous key no longer does. Refill before each test, then test N (49), top-row 5 (6), and X again. The log should report `Hotkey RegisterForKey completed` for the selected key. With sharing off, save and reload to verify per-save retention.

For the shared-settings test, choose a free hotkey and a 12-hour refill, enable Remember settings across saves and click Apply. Check the success message. Quit to desktop and load an older/different save: the menu's Current values should show the remembered choices without needing Apply again. Test a new game as well. To test opt-out, turn sharing off, Apply and restart; an older save should then retain its own values. Test re-enchantment with a phial obtained through the original quest; active refill behavior is determined by the original mod.

If the page is missing, check `WhitePhialMenu.log` in the SKSE log folder. In Windows, open `shell:Personal`, then `My Games/Skyrim Special Edition/SKSE`. The log records menu registration, resolved globals, direct setting updates and readback results. A missing setting disables the page instead of guessing a FormID.

## Build and credits

Source: [Physics-helper, codex/white-phial-menu](https://github.com/sl168898/Physics-helper/tree/codex/white-phial-menu/white-phial-menu). Run `tools/build_windows.ps1` on Windows with Visual Studio 2022 C++ tools, CMake, Git and PowerShell 7. Dependencies are pinned in the script. The GitHub Actions workflow builds and tests the same source.

- Original mod and documented settings: [AndrealletiusVIII / White Phial - Tweaks and Enhancements](https://www.nexusmods.com/skyrimspecialedition/mods/73532).
- Menu API: [QTR-Modding / SKSE Menu Framework 3 API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API), LGPL-2.1. The adapter dynamically calls the installed framework. API source is unmodified; its license is included.
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG), license included.
- Adapter source: MIT, see LICENSE.
