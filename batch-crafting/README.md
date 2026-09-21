# Batch Crafting 0.1.0 beta

Batch cooking and item creation using the selected recipe's native crafting routine.
Built for Steam Skyrim Special Edition / Anniversary Edition **1.6.1170 only**.
This is a first release: the Windows DLL and portable safety tests are built and
checked, but it has not been tested inside Skyrim.

## Requirements and installation

- Skyrim 1.6.1170 (Steam), SKSE 2.2.6 and the matching AE Address Library.
- SKSE Menu Framework **3.x** and its requirements.
- SkyUI is the intended crafting interface. Other menu replacements are untested;
  the plugin refuses a recipe when it cannot read its enabled/selection state.

Install the ZIP as a new mod in MO2, enable it on the left, and start through SKSE.
There is no ESP and no load-order position to set. Do not install on 1.5.97, GOG,
VR or other executable versions. The plugin rejects incompatible runtimes.

## Use

1. Open a cooking pot or forge and highlight the recipe you want.
2. Press **F8** to open Batch Crafting.
3. Choose **1**, **5**, **10**, **maximum**, or a custom quantity.
4. The quantity window closes and crafting runs in the normal crafting menu.
   Press F8 again to cancel, or change recipe / leave the station.

Quantity means **recipe executions**, not individual output items. If one recipe
makes 24 arrows, ten executions make 240 arrows and consume ten recipes' materials.
Each batch is capped at 1000 executions and proceeds at most four times a second.

The SKSE Menu Framework page under **Batch Crafting > Cooking and Smithing** also
has a Read selected recipe button. You must already be at the crafting station.
Edit `SKSE/Plugins/BatchCrafting.ini` in MO2 to change the keyboard key; restart
Skyrim afterward. Codes are DirectInput scan codes, not Windows virtual keys.
F7=65, F8=66, F9=67. Choose an unused key; this plugin does not suppress other mods'
key handlers. Controller users can open the Framework page using their existing
Framework controls; there is no dedicated gamepad shortcut in this release.

## Supported scope and safeguards

Cooking, forge item creation, tanning and smelting share the constructible recipe
menu and are the intended scope. Alchemy, enchanting, armor improvement and weapon
tempering use other crafting systems and are not supported.

The plugin calls the native confirmed-craft routine once per execution. It does
not grant items or XP directly, replace recipes or alter material costs. Before
each execution it checks the same menu/session/recipe, the game-populated enabled
flag, native recipe conditions and currently held ingredient counts. Afterward,
it verifies that the expected output appeared before attempting another craft.
Free recipes and recipes producing their own required ingredient are refused.
Modded recipes, including Requiem recipes, should retain their native behavior,
but compatibility with custom crafting scripts and UI replacements needs testing.

Changing the selection, closing/loading a menu, loading a save or starting a new
game invalidates the batch. Unfinished work is never saved or resumed. Materials
already spent on successful crafts are not refunded when cancelling. Keep the
selected recipe unchanged while the batch runs. A change or inventory-altering
script can cause a safe early stop; the mod does not attempt to undo other mods.
There is no Papyrus timer, saved quest or background inventory scan. Inventory
checks occur when requesting a recipe preview and during an explicitly started
batch in the crafting menu.

## First in-game check

Make a separate save before the first test. At a forge, select a cheap recipe for
which you have at least five crafts' ingredients. Craft five and check both the
outputs and consumed materials. Repeat at a cooking pot. Then test cancellation
and insufficient materials. Also check a multi-output recipe such as arrows.
The notification reports completed recipe executions, not output item count.

If the quantity window reports an unsupported interface or mismatched recipe,
do not disable the safety check. Supply the selected recipe, crafting-menu mods,
runtime, and `Documents/My Games/Skyrim Special Edition/SKSE/BatchCrafting.log`.
If a crash occurs, include the crash logger report. A successful build does not
prove native game integration or compatibility with your full mod list.

To remove, finish/cancel the batch, close Skyrim, then disable this mod in MO2.

## Build and implementation references

Source and a pinned Windows build script are included under `Source/BatchCrafting`.
Run `tools/build_windows.ps1` on Windows with Visual Studio 2022 C++ tools, CMake,
Git and internet access. The script builds the DLL and runs release-enabled tests.
The native routine ID (AE 51369) is documented by Yes Im Sure NG's public source;
no binary patch or source from that project is included here.

- CommonLibSSE-NG: https://github.com/CharmedBaryon/CommonLibSSE-NG
- Menu API: https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
- Native craft reference: https://github.com/VersuchDrei/YesImSure/blob/master/src/Hooks.cpp
- SkyUI crafting interface: https://github.com/schlangster/skyui/tree/master/src/CraftingMenu

BuildInfo.json records the source/dependency revisions, source and DLL hashes,
and the distinction between build/test verification and in-game testing.
