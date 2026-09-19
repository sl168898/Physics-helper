# Star Runestone Recipes 1.0

Forge Runemaster runestones directly from the soul stored in Azura's Star or the Black Star. The soul is consumed; the same Star stays in your inventory, empty and reusable. No empty soul gem or other ingredient is required. This mod adds recipes only.

| Ingredient in your inventory | Forge output | Star afterward |
| --- | --- | --- |
| Azura's Star with a common soul | 1 Arcane Runestone | Empty |
| Azura's Star with a greater soul | 2 Arcane Runestones | Empty |
| Azura's Star with a grand soul | 4 Arcane Runestones | Empty |
| Black Star with a grand soul | 4 Tenebrous Runestones | Empty |

The recipes use the original Runemaster 1.5 runestone items and yields. Empty Stars and petty/lesser souls do not qualify. The Black Star recipe requires a grand soul. If another mod permits a grand white soul in the Black Star, it also yields Tenebrous Runestones: Skyrim's stored soul data records size, not the original victim's race.

## Requirements

- Skyrim Special Edition, **Steam runtime 1.6.1170**.
- SKSE64 compatible with 1.6.1170 and the matching AE Address Library for SKSE Plugins.
- **Runemaster Magic 1.5**, including `RunemasterMagic.esl`.

The earlier Runemaster Enchanted Weapons patch may remain installed. This add-on is separate and does not require that patch.

## Installation and use

1. Install this ZIP as a new mod in MO2.
2. Enable the mod and **Star Runestone Recipes.esp**. Load it after `RunemasterMagic.esl`.
3. Launch through SKSE. Carry a Star containing a qualifying soul and open a forge.
4. Select the matching runestone recipe and craft once. The runestones are added and the Star becomes empty. Capture another soul before crafting again.

The forge ingredient list names the Star; the recipe checks its actual stored soul. Only the recipe matching that soul qualifies. Runemaster may label both runestone items simply "Runestone" in some interfaces; this add-on retains those original item names and models.

No spells, powers, quests, background polling, custom inventory items, or new textures are added. The ESP is ESL-flagged and contains only four new recipes plus their four private condition records. The DLL performs the soul check and preserves the Star during crafting. It does not change the Star's base record, appearance, or quest data. If the DLL fails to load, the added recipes remain unavailable.

## Validation and limits

Windows compilation, crafting transaction tests, the MSVC inventory-hook ABI check, and independent ESP record/reference checks are run by the build workflow. BuildInfo.json records the exact source revision and results. **No in-game testing has been performed in this build environment.** Save before the first craft, then verify the expected output and that your emptied Star can trap another soul.

The automated crafting checks cover all Star/soul combinations, wrong-size and empty souls, repeated attempts, reentrancy, failed output delivery with soul restoration, item identity preservation, invalid stacks, and save/load guards. They do not replace an in-game compatibility test with LoreRim/Requiem or mods that replace the crafting menu or inventory functions. Ordinary overrides to the Stars are retained. Mods that replace a Star with a different base form or store souls outside Skyrim's ExtraSoul data need a separate patch. Artificial stacks sharing one filled ExtraDataList are rejected rather than emptying multiple Stars at once.

For troubleshooting, inspect `Documents/My Games/Skyrim Special Edition/SKSE/StarRunestoneRecipes.log`. A successful initialization logs "Ready: four forge recipes". Restore the pre-install save if removing this add-on; other load-order changes are outside its control.

## Source and build

Source: https://github.com/sl168898/Physics-helper/tree/codex/star-runestone-recipes/star-runestone-recipes

The Windows script pins CommonLibSSE-NG and vcpkg revisions, compiles with Visual Studio 2022 x64, runs tests, generates the ESP, and packages the DLL and documentation. The native condition adapter intercepts only this plugin's four private GetGlobalValue markers and chains all unrelated calls. The inventory hooks act only while one of these four recipes is selected, and chain unrelated operations. The Star itself is never removed/recreated; only the selected instance's ExtraSoul value is emptied and marked for saving.
