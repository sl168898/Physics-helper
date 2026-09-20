# White Phial - Decanting 1.2

Preserves the name of a renamed potion or poison when it is assigned to the White Phial and subsequently decanted. The new bottle receives its own display name, which is available to inventory and the existing Wheeler potion-name compatibility patch.

## Requirements

- Skyrim Special Edition / Anniversary Edition **Steam 1.6.1170** and SKSE64 2.2.6.
- Address Library for SKSE Plugins, Anniversary Edition version.
- [The White Phial - Tweaks and Enhancements](https://www.nexusmods.com/skyrimspecialedition/mods/73532) and its requirements.
- Keep Rename Potions SKSE and your existing Wheeler / Wheeler Refined compatibility patch enabled if you use them. This update does not include or replace their DLLs.

The native helper is deliberately restricted to runtime 1.6.1170. Other runtimes require a separate verified build.

## Installation and the first named dose

1. Close Skyrim. Replace the previous **White Phial - Decanting** addon in MO2 with the complete `White_Phial_Decanting_v1_2.zip`. Keep only one version of this addon enabled.
2. Keep `White Phial - Decanting.esp` enabled after `The White Phial - Tweaks and Enhancements.esp`. Keep the original phial mod, Rename Potions, your Wheeler compatibility patch, and any White Phial Menu / Widget addons enabled.
3. **Assign a renamed bottle to the White Phial again, once after installing this update.** Select the intended named batch in the original mod's assignment menu. The helper records its name before that bottle is consumed.
4. Let the phial refill and use **Decant White Phial**. Check the newly produced bottle in inventory, then add that bottle to Wheeler. If a wheel slot still represents an older unnamed batch, remove that slot and add the new named bottle.

No new game or quest reset is required. A phial filled before this update has no captured name; assigning a named bottle again supplies it. Existing unnamed bottles are not retroactively renamed.

## Preserved behavior

Manual decanting still produces one dose and starts the original refill cycle. The lesser power still requires a full, fully re-enchanted phial. The optional daily 8 AM setting in White Phial Menu v5 uses the same named-dose path. The original duplicate-phial, effect-matching and poison-counter checks remain in place.

The selected bottle's effects, magnitudes, durations, model, weight and value still come from the original saved potion. Only the new bottle's display name is applied. Separate named batches can retain separate names even when their underlying potion form is identical. Selecting an ordinary batch clears the stored custom name for future doses.

Names are saved in the SKSE co-save and restored with form-ID resolution. Keep the matching `.skse` file with its game save. Names are cleared when reverting to another save or character. This update addresses lost names; it does not rebuild or provide independent persistence for dynamically created potion forms.

## Implementation

`WhitePhialNames.dll` hooks the player's item-removal function and captures the selected potion's inventory display name during the original phial's GiftMenu transfer. It recognizes the original assignment flag, mod-owned destinations, and the phial service quest's reference aliases. Outside this context it calls through without examining inventory lists. It does not run a polling timer.

`WPD_Names.GetBottleName` returns the captured name only for the currently recorded potion form. `WPD_DecantQuest.GiveBottle` creates one disabled reference, applies its display name, and moves that reference into the player's inventory. It does not rename the shared potion form or recreate potion effects. If no name is captured, the helper is absent, or applying the name fails, decanting uses the original ordinary-bottle grant.

The original addon ESP, startup SEQ, player-alias script and power-effect script are retained byte for byte from v1.1. The original mod's scripts are not overridden. Compile-only declarations for those scripts are not shipped as game scripts.

## Verification and in-game check

The release includes build and payload validation reports. Automated tests cover potion identity, switching between differently named batches, clearing an ordinary name, Unicode text, form-ID remapping, invalid saved data and save isolation. The updated Papyrus bytecode is inspected for the single-dose named grant and unchanged refill path. A Windows x64 build is required before packaging.

**This update has not been tested inside Skyrim.** The native GiftMenu capture and the game's transfer of the named reference require an in-game check:

- Assign a renamed potion, decant it, and check inventory and Wheeler.
- Repeat with a renamed poison, including a name such as `Kindling Oil`.
- Use the last remaining named bottle for assignment; save/reload before decanting and confirm its name survives.
- Select a second named batch with the same effects, then an ordinary batch; confirm new doses follow the latest selection while previous bottles keep their names.
- If using the daily option, check the next 8 AM dose too.

For diagnosis, `Documents/My Games/Skyrim Special Edition/SKSE/WhitePhialNames.log` records helper startup, `Selected phial liquid` on capture, and `Naming decanted dose` on delivery. A startup line alone does not prove that a bottle's name was captured.

## Building and credits

Run `tools/build_windows.ps1` with Visual Studio 2022 C++ tools, CMake, PowerShell 7, Git and network access. It pins CommonLibSSE-NG and vcpkg, builds the native DLL, runs the name-state tests, and records source and DLL hashes. The complete addon also requires the compiled Papyrus scripts and unchanged v1.1 ESP/SEQ.

Papyrus compilation uses [russo-2025/papyrus-compiler](https://github.com/russo-2025/papyrus-compiler), release 2026.03.15, with Skyrim/SKSE declarations and compile-only interfaces for the original phial scripts.

- Original phial mod: AndrealletiusVIII, [The White Phial - Tweaks and Enhancements](https://www.nexusmods.com/skyrimspecialedition/mods/73532).
- Naming integration checked against [Rename Potions SKSE source](https://github.com/alexjiang200407/rename-potions-skse), revision `cb7760745945bf103ed5bb761538253cc5bdd60b`.
- [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG), revision `b93280e832f263dbef44e44cbe2936622a02f91a`; its license is included.
- This helper and its sources are provided under the included MIT license. No original phial-mod, Rename Potions, or Wheeler binaries are redistributed by this update.
