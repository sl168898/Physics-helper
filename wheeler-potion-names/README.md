# Wheeler Refined Rename Potions + I4 patch v1.2

This update preserves the v1.0 potion-name patch and the newer Wheeler changes bundled
with Enchantment_Swapper_Description_Fix_v1_1_0.zip. It retains per-batch names,
counts, activation and saved wheel bindings. The new icon bridge passes that same
batch name to I4 and caches icon choices separately for different names on one
base potion form. It also keeps the selected texture descriptor alive while drawing.

The combined distribution includes RenamePotions_I4.esp, its I4 configuration,
and original sword-and-oil-drop artwork in SWF and SVG form. The plugin is an empty,
ESL-flagged configuration loader with no gameplay records. It depends on I4IconAddon.esp.
The SVG lets Wheeler draw this icon directly with I4's color, without enabling SWF extraction.

## Requirements

- Steam Skyrim SE/AE 1.6.1170 with SKSE and Address Library.
- The existing Wheeler setup, with Wheeler Refined 1.3.3.0 installed.
- Rename Potions SKSE 1.0.0, SkyUI, Inventory Interface Information Injector 1.1.0
  and its I4IconAddon.esp from the supplied I4 archive.

## Install

1. Install the combined ZIP as a new MO2 mod after Wheeler Refined, Rename Potions,
   I4, the old Wheeler Rename Potions v1.0 patch, and Enchantment Swapper Description Fix v1.1.0. This mod must win the wheeler.dll conflict.
   You can disable the standalone v1.0 name patch because its changes are included.
   Keep Enchantment Swapper Description Fix enabled: its helper DLL and scripts are still
   needed for transferred enchantment descriptions. Only its wheeler.dll is superseded.
2. Enable RenamePotions_I4.esp after I4IconAddon.esp and after other potion icon rule plugins
   whose name rules you want this patch to override.
3. The included SKSE/Plugins/wheeler/I4.ini enables I4. If you already have a customized
   I4.ini, merge Enabled=true, PreferI4Icons=true, UseForPotions=true and UseForPoisons=true
   into your winning file. ExtractionMode is not required for the included oil icon.
4. Fully restart Skyrim through SKSE. Existing v1.0 named wheel slots keep their bindings.
   A slot bound to an old name will not automatically guess a new name: remove and re-add
   the potion from its specific inventory row after renaming it again.

## Names and icons

| Example name | Icon | Color |
| --- | --- | --- |
| Weapon Oil, Weapon Oils, Blade Oil | Sword and oil drop | Amber |
| Fire Oil, Flame Oil | Sword and oil drop | Red-orange |
| Frost Oil, Ice Oil | Sword and oil drop | Ice blue |
| Shock Oil, Lightning Oil | Sword and oil drop | Yellow |
| Healing Potion, Health Potion | Health potion | Pink-red |
| Magicka Potion, Mana Potion | Magicka potion | Blue |
| Stamina Potion | Stamina potion | Green |
| Poison, Venom, Toxin | Poison | Purple |
| Fire/Frost/Shock Resistance Potion | Corresponding resistance potion | Element color |

Rules apply to player-crafted dynamic potions and poisons by their inventory display name.
They change display icons only; renaming a drinkable potion to Weapon Oil does not turn it
into a weapon poison. Other names retain their normal icon behavior.

I4 1.1 uses exact, case-sensitive matching. Common title/lower/upper/sentence-case names,
strength prefixes (for example Strong Weapon Oil), and rank suffixes (Weapon Oil II)
are included. Arbitrary extra words, punctuation or combined prefix-and-suffix variants
must be added explicitly. The full list is in Documentation/RenamePotionsI4/Supported-Names.txt.
To add a name, edit SKSE/Plugins/InventoryInjector/RenamePotions_I4.json and add the exact
name to the desired rule's match.text.anyOf array, preserving valid JSON. Restart the game.
Do not rename the ESP or JSON: I4 associates them by filename.

## In-game check

Craft two batches sharing the same recipe/base form, name one Weapon Oil and another
Healing Potion, and put both specific inventory rows on the wheel. Check each name,
count and icon in inventory and Wheeler, save, quit and reload. Each should keep its
own identity. Use one batch and check only its count decreases; when depleted it must
not switch to the other named batch. Use a crafted poison for an actual weapon oil.

Validation status is recorded in BUILD-INFO.json and VALIDATION.json. Native compilation
and automated checks do not substitute for an in-game test; Skyrim was not available here.

## Source and rebuild

Source/Wheeler-Refined contains the exact modified source. Source/tools/build_windows.ps1
pins the original Refined, CommonLib and vcpkg revisions and applies rename-potions.patch
then i4-named-icons.patch. Source/build_patch.py regenerates the I4 assets. License and
third-party notices are included. This is a modified compatibility build, not an official
Wheeler Refined release.

## Preserved Enchantment Swapper compatibility

The weapon and armor donor-description bridge from Enchantment Swapper Description Fix
v1.1.0 is included byte-for-byte. The helper remains optional and its API is discovered
at runtime. If you use that fix, keep its EnchantmentSwapperDescriptions.dll and scripts
installed. This archive updates Wheeler only; it does not replace the helper or scripts.
The recorded transfer description, including numeric formatting supplied by the helper,
continues to take precedence over the generic weapon/armor description.

## Preserved v1.0 behavior

The selected inventory batch name is saved in Wheeler's existing SKSE co-save and used
for labels, counts and activation. Different names on the same potion remain separate.
No inventory stack pointer is retained between uses. Renamed poisons retain their name
through deferred weapon application. Depleted slots do not borrow another batch's items.
The original last-copy protection for dynamic forms remains in place when Clear Depleted
Consumables is disabled. Legacy form-only slots adopt a name only if all available copies
have one unambiguous custom name; otherwise remove and re-add the intended inventory row.

The startup log identifies this build as:
`Wheeler - Refined Rename Potions + I4 compatibility patch v1.2 enabled (2026-09-28)`.
The underlying Wheeler version remains Refined 1.3.3.0; v1.2 is this compatibility patch's version.

## Credits and license

Wheeler Refined by C0kadam: https://github.com/c0kadam/Wheeler-Refined,
revision e3360bf81d05f739d2caa94d50e64e174b0ce1f8 (v1.3.3).
Original Wheeler by dTry/D7ry, with its BSD-3-Clause notice preserved.
Rename Potions SKSE by shdowraithe101/alexjiang200407:
https://github.com/alexjiang200407/rename-potions-skse.
I4 by Exit-9B: https://github.com/Exit-9B/InventoryInjector.
SkyUI by the SkyUI team: https://github.com/schlangster/skyui.

The modified Wheeler build and compatibility code are GPL-3.0-only; notices and full modified
source are included. Rename Potions, I4 and SkyUI are dependencies and their binaries are
not redistributed. The oil glyph is original artwork created for this patch, supplied under
CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/). Modification date: 2026-09-28.
