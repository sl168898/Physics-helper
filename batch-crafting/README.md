# Batch Crafting 0.2.0 beta — quantity crafting and SCIE support

Cooking and forge recipe quantities for Steam Skyrim **1.6.1170**.

## What changed from 0.1.0

The old release ran an individual craft every 250 ms and counted only the player's
inventory. Both behaviors have been replaced:

- Each quantity selection calls the game's confirmed-craft routine **once**.
  Ingredient removal, output addition and native smithing skill-use amounts are
  scaled for the selected quantity during that call. There is no auto-click loop,
  per-item timer, repeated crafting animation sequence or batch resume state.
- With **SCIE 2.6.0** installed, material counts come from the exact crafting count
  entry point SCIE hooks. Its total already includes the player. Ingredient removal
  chains through SCIE's existing handler, which chooses the contributing sources.
  SCIE's active station filters and source selection remain in charge.
- The preview identifies whether it is using the player or SCIE shared inventory.
- Batch totals are checked after crafting and logged. An unexpected accounting
  result disables further batches until restarting Skyrim, without an automatic
  retry or speculative refund.

The Windows plugin builds and automated accounting tests pass. Native Skyrim
integration and compatibility with your complete mod list still need in-game
verification; this remains a beta.

## Install / update

Requirements: Steam Skyrim 1.6.1170, SKSE 2.2.6, matching AE Address Library,
SKSE Menu Framework 3.x and its dependencies. SkyUI is the intended crafting UI.
SCIE is optional; the integration was inspected against the supplied 2.6.0 build.
Other game runtimes, VR/GOG, and other SCIE versions have not been validated.

1. Close Skyrim.
2. Replace/disable Batch Crafting 0.1.0 and install this ZIP through MO2. Keep only
   one `SKSE/Plugins/BatchCrafting.dll` active.
3. Leave your existing SCIE mod enabled. This package contains **no replacement
   SCIE files**, ESP, Papyrus script or recipe records.
4. Launch through SKSE.

If MO2 asks for a data directory, choose the archive root containing `SKSE`.
There is no plugin load-order position to set and no new game requirement.

## Use

Highlight a recipe at a cooking pot or forge, press **F8**, then choose 1, 5, 10,
maximum, or a custom quantity. The result is crafted together in one operation.
Close the quantity window to cancel before confirming. A submitted batch runs
synchronously; there is no between-items cancellation phase.

Quantity means recipe units. Ten units of a recipe that normally makes 24 arrows
produce **240 arrows** and use ten recipes' materials. The cap is 1000 units; integer
limits may lower it for unusual recipes. Free/self-producing recipes are refused.

The Framework page is **Batch Crafting > Cooking and Smithing**. Use Read selected
recipe while already at a crafting station. Edit `SKSE/Plugins/BatchCrafting.ini`
to change F8 (DirectInput code 66); restart afterward. Choose a key unused by other
mods. F7=65 and F9=67. There is no dedicated gamepad shortcut.

Cooking, forging, tanning and smelting use the supported constructible recipe
menu. Alchemy, enchanting and equipment tempering are outside this release's scope.
The recipe's output and conditions are kept; no permanent recipe data is edited.

## SCIE integration details

SCIE's public inventory message (`SCRI` / `SCPI`) is used to verify an active
crafting session. It supplies only a summary, not per-item stock. Actual material
counts use AE Address Library function **16109**, the entry patched by SCIE's
`Hook_GetInventoryItemCount`. We never add pocket counts to its combined total,
read SCIE's private memory, guess eligible containers, or move items into pockets.

The player RemoveItem vtable hook chains through the prior hook, including SCIE's
container-first removal. No change to SCIE's INI, filters, registered containers,
cosave or DLL is needed. If SCIE is present but its session cannot be confirmed,
batching refuses to start instead of silently falling back to pockets.

## Behavior and compatibility limits

The quantity multiplier is thread-local and active only inside the single native
craft call. It is suspended while forwarding inventory/skill hooks, so nested
callbacks and normal gameplay operations cannot inherit it. Native UseSkill and
AddSkillExperience paths are covered without multiplying twice. Only smithing
skill amounts are scaled; custom skill systems may need their own integration.

Inventory events carry total quantities. The vanilla ItemCrafted event has no
quantity field: when one matching native event is observed and accounting succeeds,
additional craft **notifications** are emitted for the remaining recipe units.
This supports listeners expecting one notification per recipe. It does not run
additional crafts, sounds or material consumption. Mods with asynchronous or
private crafting hooks still require testing; universal compatibility is not claimed.

Conditions and quantities are rechecked at submission. No queued operation survives
closing the crafting menu, changing its selection or loading a save. The plugin
has no inventory polling timer, Papyrus quest, serialization, or saved recipe changes.

## Focused in-game test

Use a separate save for the first trial. Start with a cheap five-unit recipe:

1. Put **all** its required materials in a SCIE-enabled container. Carry none.
   Open the station: the Batch Crafting window should say it uses SCIE sources,
   show the correct maximum, and craft five together.
2. Verify output and container ingredient totals. Repeat with materials split
   between two sources and your inventory; the displayed maximum must not count
   the player twice.
3. Test one cooking recipe and one forge recipe; check smithing XP as well.
4. Test a multi-output recipe (e.g. arrows). Five units of a 24-arrow recipe must
   produce 120 arrows.
5. Disable a SCIE source or change its station filters, reopen the station and
   confirm it is no longer counted. Close the quantity window to test cancellation.

If something is wrong, stop testing that save and send
`Documents/My Games/Skyrim Special Edition/SKSE/BatchCrafting.log` plus SCIE's log.
The batch log includes source mode, expected/observed ingredient debit, output
change and the number of intercepted native calls. Include the recipe and amounts
before/after. If there is a crash, include the crash logger report.

To uninstall, close Skyrim and disable the mod in MO2.

## Source / build

Source and pinned `tools/build_windows.ps1` are included under `Source/BatchCrafting`.
The script needs VS 2022 C++ tools, CMake, Git and internet access. It compiles the
adapter first, builds the DLL, runs release-enabled accounting tests and packages
only this plugin. BuildInfo.json records revisions, hashes and verification scope.

References (upstream source reviewed, not redistributed SCIE assets):

- SCIE by OhFor: https://www.nexusmods.com/skyrimspecialedition/mods/170497
- SCIE source: https://github.com/ohfor/scie/tree/558ddd0c8026cb16bac0fb157cde23d9c0a3edb2
- CommonLibSSE-NG: https://github.com/CharmedBaryon/CommonLibSSE-NG
- Menu API: https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API
- Native craft routine reference: https://github.com/VersuchDrei/YesImSure/blob/master/src/Hooks.cpp

CommonLib, Menu Framework API and MinHook license notices are bundled.
