# Soul Gem Recharge — Manual Mode and SKSE Menu

Version 1.2.0 add-on, including William_Rod's original Soul Gem Auto Recharge 1.1.0 core.

This edition recharges equipped enchanted weapons using filled soul gems. It does not fill empty soul gems.

## Requirements

- Steam Skyrim Special Edition **1.6.1170**.
- SKSE64 and the matching AE Address Library for SKSE Plugins.
- [SKSE Menu Framework 3.18 or newer](https://www.nexusmods.com/skyrimspecialedition/mods/120352), with the requirements listed on its page.
- Your existing SkyUI installation. Its old quest/script identity is retained to remove the previous MCM entry safely.

Other Skyrim executable versions, VR, and GOG builds are not supported by this add-on.

## Install with MO2

1. Save and exit Skyrim before changing the installation.
2. Install this ZIP as a new mod. Place it after **Soul Gem Auto Recharge 1.1.0** in MO2's left pane and let this package win file conflicts. The original core is included, so a separate original installation is optional.
3. Keep `AGH_SoulRecharge.esp` enabled. Its name and quest FormID are unchanged.
4. Start Skyrim through SKSE. Load your save, then open **SKSE Menu Framework** with the framework's menu hotkey. Open **Soul Gem Recharge → Settings**.

This package replaces the settings page in MCM. An already registered MCM entry is retired on SkyUI's next ready event; allow up to about 30 seconds after loading the save. It does not replace SkyUI's menus.

## Use

- **Automatic recharge OFF:** Manual mode. Automatic requests from this mod are blocked before soul consumption. Press your recharge hotkey while playing to run the original manual recharge command.
- **Automatic recharge ON:** Automatic mode. The original automatic recharge behavior and charge threshold are active. The manual hotkey also remains available.
- Click **Recharge hotkey**, then press a keyboard key. Escape cancels. The initial key is **F11** with the supplied original INI. Choose a key that does not conflict with your game controls, other mods, or the framework's own menu key.
- Mode and hotkey changes save immediately and take effect without restarting Skyrim.
- Other options use **Apply weapon and soul-gem settings**. The master **Enable weapon recharging** option must be enabled for either mode to work.
- **Refresh current settings** reads back the original mod's saved settings and last result. It discards unapplied edits to those settings.

Manual mode is the default on first installation. A full weapon, an unsupported weapon, or the absence of an eligible filled gem can make the original manual command do nothing. This edition preserves the original 1.1.0 command's amount, hand-selection behavior, soul protections, reusable-gem handling, and restrictions.

The hotkey is handled on key-down, with repeats suppressed. It is inactive while loading, while the game is paused, in the console, and while a blocking SKSE Menu Framework window is open. The binding accepts a single keyboard key; mouse/gamepad bindings and key combinations are not included.

The original F10 diagnostic command remains available unless you assign that key to recharging. After rebinding recharge, the old recharge key no longer triggers this mod. Input is remapped privately inside this mod's own handler; other mods do not receive a synthetic F11 keypress.

## Preserved settings and features

The original DLL is included unchanged. Its weapon support, soul selection, reusable Azura's Star/Black Star behavior, XP/stat updates, extra-gem INI entries, and protection checks are retained.

Your existing `SKSE/Plugins/AGH_SoulRecharge.user.json` is **not included or overwritten**. The replacement menu uses the original native settings API to read and apply those settings. Mode and binding use the separate generated file `SKSE/Plugins/SoulRechargeMenu.json`. MO2 may place generated settings in Overwrite; keep those files when updating.

This switch governs **Soul Gem Auto Recharge** only. Other mods, enchantments, and perks with their own recharge effects continue to follow their own rules.

## Troubleshooting

- No menu: install SKSE Menu Framework 3.18+ and its dependencies. If it is absent, the saved mode and hotkey still work, but its settings page cannot be displayed.
- No recharge: confirm the master enable option, supported equipped weapon, eligible filled gems, and soul protection settings. Press Refresh to see the original mod's last result.
- Bridge warning: use the exact original DLL included here. This add-on checks its SHA-256 and hook instructions before modifying memory. A different upstream version requires an updated bridge; it is deliberately not patched speculatively.
- Diagnostics: `Documents/My Games/Skyrim Special Edition/SKSE/SoulRechargeMenu.log`, plus the original mod's log.

To revert this update when the original mod remains installed below it, disable this package and restore the original files. Keep the ESP enabled. Do not remove unrelated settings files or clean your save as an installation step.

## Validation and limitations

The Windows build runs the **actual original recharge kernel** with controlled callbacks to verify that Manual mode rejects automatic requests before consumption, manual requests still run, automatic behavior can be restored, dry-run handling remains intact, and blocked requests do not latch an engine error. It also checks hotkey routing and incompatible-DLL rejection.

These tests do not run Skyrim or validate real in-game inventories, UI rendering, controller interaction, or save migration. **In-game testing remains necessary.** See `BuildInfo.json` for the built artifact and test results.

## Credits and source

- [Soul Gem Auto Recharge](https://www.nexusmods.com/skyrimspecialedition/mods/192028) by **William_Rod** — original MIT-licensed DLL, ESP, native script, configuration, and documentation. The root `LICENSE`, original third-party notices, and `Docs/Original-README.md` are retained.
- Manual-mode bridge, keyboard remapping, settings page, and MCM retirement script: Physics-helper contributors, MIT license (`licenses/SoulRechargeMenu-MIT.txt`).
- SKSE Menu Framework API by QTR Modding / SkyrimThiago, LGPL-2.1. Header and license are included with the build sources. The framework itself is a separate dependency.
- CommonLibSSE-NG and its dependency licenses are included in `licenses`.

Reproducible native sources are under `Source/SoulRechargeMenu` and in the `codex/soul-recharge-manual-menu` branch of `sl168898/Physics-helper`. See `BRIDGE-NOTES.md` for the compatibility boundary.
