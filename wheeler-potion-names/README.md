# Wheeler Refined Rename Potions + I4 patch v1.1

This update builds on the supplied v1.0 potion-name patch. It retains per-batch names,
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
   I4, and the old Wheeler Rename Potions v1.0 patch. This mod must win the wheeler.dll conflict.
   You can disable the old v1.0 patch because its changes are included.
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

## Original v1.0 behavior and build notes

# Wheeler Refined — Rename Potions compatibility patch 1.0

Shows the inventory name assigned by Rename Potions SKSE on Wheeler Refined.
Built for the supplied Wheeler Refined **1.3.3.0** and Skyrim Steam **1.6.1170**.
This is a modified build of Wheeler Refined's published v1.3.3 source.

## Installation in Mod Organizer 2

1. Close Skyrim.
2. Install this ZIP as a separate mod named `Wheeler Refined - Rename Potions Patch`.
3. Place it below Wheeler and Wheeler Refined in MO2's left panel. This patch must
   win the conflict for `SKSE/Plugins/wheeler.dll`.
4. Keep Rename Potions SKSE, original Wheeler, Wheeler Refined, and their existing
   dependencies enabled. Launch through SKSE as usual.
5. In your inventory, highlight the renamed potion, open Wheeler in edit mode,
   and add it to the desired slot. Save normally to retain the binding.

There is no ESP, extra hotkey, MCM page, or background polling script.
Your current Wheeler settings and visual assets are not included in this patch.

## What changes

- Slot labels and the highlighted item's name use the selected inventory batch's
  name instead of its shared base potion name.
- Differently named batches of the same potion can occupy separate wheel entries.
  Their counts and activation use the matching name, with a fresh inventory lookup
  immediately before use. Names are compared exactly, including case.
- The batch name is stored in Wheeler's existing SKSE co-save data. No pointer to
  an inventory stack is saved or retained by the patch between uses. Rendered
  names are copied when Wheeler refreshes its existing inventory snapshot, so
  drawing does not dereference a stack that has since been consumed.
- Renamed poisons carry their name through Refined's deferred poison-use flow.
- Existing slots upgrade automatically when all owned copies of that potion have
  one custom name. If you own several differently named batches, **remove the old
  wheel entry and re-add the desired inventory row**. An old form-only save cannot
  reveal which batch you intended.
- Depleted batches follow Refined's existing missing-item settings; they do not
  borrow another batch's name or count. The original protection against consuming
  the last copy of a dynamic potion when Clear Depleted Consumables is disabled
  remains in place.

If you change a batch's name after adding it to the wheel, remove and re-add that
entry. Distinct dynamically generated potion forms remain distinct even when they
share a name; this patch does not merge recipes or restore deleted dynamic forms.

## Checking it in game

Add a renamed potion and confirm both its wheel label and highlighted name.
Save, fully exit, and reload to confirm the label remains. For two named batches
of the same potion, add both and consume one; only that batch's count should fall.
Test a renamed poison too if you use them. The build and automated logic checks
are verified separately; **Skyrim gameplay has not been tested here**.

The log includes `Wheeler - Refined Rename Potions compatibility patch v1.0 enabled`.
Do not combine this replacement DLL with a different Wheeler DLL patch unless
its source changes have been merged. A future Refined update needs a fresh build.

To remove this patch, disable it in MO2 so the original Refined DLL wins again.
Original Wheeler ignores the additional JSON name field; re-add affected slots
if needed after returning to the unpatched version.

## Source and credits

- Wheeler Refined by C0kadam: https://github.com/c0kadam/Wheeler-Refined
  revision `e3360bf81d05f739d2caa94d50e64e174b0ce1f8` (tag `v1.3.3`).
- Original Wheeler by dTry/D7ry, with its BSD 3-Clause notice preserved.
- Rename Potions SKSE by shdowraithe101/alexjiang200407:
  https://github.com/alexjiang200407/rename-potions-skse
  Its inventory `ExtraTextDisplayData` is read through Skyrim's normal API;
  its DLL, recipes, and serialization are not modified or redistributed.

The combined Wheeler build and this patch are GPL-3.0-only. Full modified source,
the diff, build scripts, tests, pinned dependency revisions, and license notices
are included under `Source` and `Licenses`. Modification date: 2026-09-19.
