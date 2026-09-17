# White Phial Menu

An SKSE Menu Framework settings page for **White Phial - Tweaks and Enhancements** by AndrealletiusVIII. Built for **Skyrim SE/AE 1.6.1170 on Windows**.

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

Save the game after applying changes. The engine handles these as ordinary console-set globals belonging to that save. This adapter has no shared INI override, automatic defaults, custom save serialization, or Papyrus replacement. Installing it does not change your settings. Removing the DLL removes the menu; values already saved retain their normal game behavior.

## Implementation

The adapter locates unique `TESGlobal` records by their retained EditorIDs and checks their originating plugin. It does not depend on load-order FormIDs or an EditorID-retention extension. All reads and changes run through SKSE game-thread tasks; the render callback uses a locked snapshot. Stale edits are rejected if another script changed that field, and pending work is invalidated on save loads.

For each changed field the native game command compiler runs one fixed, validated `set` command. This follows the same engine path as the author's documented console controls and retains normal save behavior. ConsoleUtil is not required and the console does not need to be open.

| Menu control | Original global |
| --- | --- |
| Fully re-enchanted | `TWPTE_PhialIsFullyRepaired` |
| Refill time | `TWPTE_ResetHours` |
| Use phial hotkey | `TWPTE_HotkeyButton` |

No original mod assets, ESP edits, new inventory items, quests, or Papyrus scripts are included.

## Validation and first in-game check

The package build compiles the DLL on Windows and runs native tests for exact command generation, invalid numeric values, partial edits, conflicting changes and save isolation. **It has not been tested in a running Skyrim instance.** BuildInfo.json records the source revision, dependency revisions and DLL hash.

For the first test, change the hotkey to a free key and refill time to 12 hours, click Apply, and check the Current values. Close the panel and try the filled potion phial. Save, quit to desktop and reload to confirm the settings remain. Finally, load another character's save and confirm its settings are read independently. Test re-enchantment with a phial obtained through the original quest; active refill behavior is determined by the original mod.

If the page is missing, check `WhitePhialMenu.log` in the SKSE log folder. In Windows, open `shell:Personal`, then `My Games/Skyrim Special Edition/SKSE`. The log records menu registration, resolved globals, applied commands and readback results. A missing setting disables the page instead of guessing a FormID.

## Build and credits

Source: [Physics-helper, codex/white-phial-menu](https://github.com/sl168898/Physics-helper/tree/codex/white-phial-menu/white-phial-menu). Run `tools/build_windows.ps1` on Windows with Visual Studio 2022 C++ tools, CMake, Git and PowerShell 7. Dependencies are pinned in the script. The GitHub Actions workflow builds and tests the same source.

- Original mod and documented settings: [AndrealletiusVIII / White Phial - Tweaks and Enhancements](https://www.nexusmods.com/skyrimspecialedition/mods/73532).
- Menu API: [QTR-Modding / SKSE Menu Framework 3 API](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API), LGPL-2.1. The adapter dynamically calls the installed framework. API source is unmodified; its license is included.
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG), license included.
- Adapter source: MIT, see LICENSE.
