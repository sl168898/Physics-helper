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
