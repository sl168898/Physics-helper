# Enchantment Swapper — Description Preservation 1.0.0

Private-use compatibility patch for CDCooley's Enchantment Swapper 2.0 and
Skyrim Special Edition Steam 1.6.1170. Requires SKSE64 2.2.6 and the AE Address
Library. Keep the original EnchantmentSwapper.esp enabled.

## What this fixes

The original transfer script moves the enchantment form and charge. Some
artifacts and modded items keep their displayed enchantment explanation in the
weapon/armor's DESC field instead of the enchantment's individual magic effects.
Moving that enchantment cannot carry the DESC field to its new item.

This helper records the original description source for the receiving item,
then fills Skyrim's item-card `effects` text with that source's description.
It preserves localization and the currently winning source record. Transfers
between subsequent items retain the original source. Ordinary generated effect
descriptions remain in Skyrim's control when there is no custom donor text.

The patch does not replace enchantment forms, magic effects, magnitudes,
durations, charge, restrictions, prices, meshes, shaders, or perks. It does not
make item-bound scripts/perks transferable; those are an original-mod limitation.
It copies the donor's whole custom DESC text; any flavor text or descriptions
of non-transferable item scripts in that field will also be displayed.

## Install the complete patch ZIP

1. Close Skyrim. Install the final patch ZIP as a separate MO2 mod.
2. Put it below Enchantment Swapper in MO2's left pane. It must win the conflict
   for `scripts/cdc_enchantmentswapperscript.pex`.
3. Keep the original mod enabled and start Skyrim through SKSE.
4. Transfer an enchantment normally, then inspect the receiving item.

No additional ESP, MCM, power, hotkey, or new game is required. This patch does
not replace or compact the original ESP. Do not independently switch original
Enchantment Swapper versions on an existing save.

The native build artifact alone is not the complete patch: it needs the locally
compiled override of the user's supplied transfer script and `ESD_Native.pex`.
The original mod and modified full script are not published in this repository.
`tools/patch_script.py` applies the minimal edit to a supplied original source.

## Existing transferred items

When an already transferred item has blank effect text, the helper can recover
it if every original weapon/armor of that type using the exact enchantment has
the same nonempty custom description. If sources disagree, it leaves the text
alone instead of choosing the wrong artifact. Existing nonblank descriptions
without a recorded donor are left alone. Exact donor history from before this
patch cannot always be reconstructed; test a fresh transfer from the original
enchanted item in that case.

## Persistence and compatibility

Description bindings belong to the save's SKSE co-save. Keep the `.skse` file
beside its matching `.ess`. Item identities use the engine's ExtraUniqueID;
existing IDs are retained. Identity-change events migrate bindings when the
engine changes an item's inventory identity. Base object and enchantment IDs
are checked before using a binding. Load-order IDs are resolved on load; state
is cleared on revert so another character cannot inherit it.

There is no periodic timer, inventory polling, or recurring Papyrus update.
Work happens on transfer, item-card construction, identity-change events, and
save/load. A one-time catalog is built when game data loads. SkyUI receives the
standard item-card field, and no SWF files are replaced. Third-party menus that
construct their own descriptions without Skyrim's item-card function are not
covered by this hook. This build deliberately supports only runtime 1.6.1170.

Disabling this patch restores the original display behavior. Enchantments and
items remain managed by the original mod/game. Disabling it and saving can lose
its donor-history data, so keep the patch enabled for persistent descriptions.

## Verification and troubleshooting

Build checks: Windows Release DLL, metadata tests, Papyrus compilation, and
archive contents. No in-game test has been run by the patch author.

Suggested game test: transfer a known custom-description enchantment; inspect
it in inventory; save/reload; place it in a container and retrieve it; transfer
the enchantment a second time. Also confirm a plain unenchanted copy retains
its ordinary description and a normal player-made enchantment still lists its
effects. Existing enchantment function/charge should behave as before.

The log is `Documents/My Games/Skyrim Special Edition/SKSE/EnchantmentSwapperDescriptions.log`.
Look for `Ready`, `Preserved description`, `Item card restored`, and
`Loaded ... description bindings`. If it fails, send that log and the exact
donor/recipient item names (or form IDs), preferably with before/after screenshots.

## Source references

- Uploaded Enchantment Swapper 2.0 source: `SwapEnchantment` calls
  `SetEnchantment`/`SetItemCharge`, but does not move a description.
- [SKSE SetEnchantment](https://github.com/ianpatt/skse64/blob/25b72352adb6543fa6d0bd3795780672b2e238e0/skse64/PapyrusObjectReference.cpp)
- [ItemCard PopulateInformation ABI](https://github.com/KernalsEgg/SKSE64Plugins/blob/9060187900d3df632d010127ac612fafc6775e5f/Shared/Shared/Source/Shared/Skyrim/I/ItemCard.cpp)
- [ItemCard address IDs](https://github.com/KernalsEgg/SKSE64Plugins/blob/9060187900d3df632d010127ac612fafc6775e5f/Shared/Shared/Source/Shared/Skyrim/Addresses.cpp)

CDCooley retains rights to the original mod. This personal overlay requires
that mod and does not redistribute its ESP, UI assets, or book. The new helper's
license does not grant permission to republish CDCooley's scripts or mod.
