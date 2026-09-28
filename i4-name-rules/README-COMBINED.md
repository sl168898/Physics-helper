# Wheeler Refined + Rename Potions + I4 compatibility v1.3

Poison names containing **weapon oil**, anywhere and in any capitalization, now
receive the sword-and-oil-drop icon and **Weapon Oil** in SkyUI's Type column.
For example, **Weapon Oil of Jarrin Crown** matches automatically. Extra words,
strength prefixes and rank suffixes are supported without adding the whole name
to a list. The English phrase uses one space; `WeaponOil` and `Weapon-Oil` do not match.

The item must actually be a poison. Its effects, strength, duration and weapon
application are unchanged. The rule also covers poison forms supplied by mods,
not just player-crafted forms. Existing exact-name aliases and elemental colors
remain available in Documentation/RenamePotionsI4/Supported-Names.txt.

## Install

1. Keep your existing Steam Skyrim 1.6.1170, SKSE, Address Library, SkyUI,
   Rename Potions SKSE 1.0.0, Wheeler Refined 1.3.3.0 and I4 installation.
   Install this ZIP as a mod and disable the older combined v1.2 patch.
2. This mod must win file conflicts for **both** `SKSE/Plugins/InventoryInjector.dll`
   and `SKSE/Plugins/wheeler.dll`, plus its JSON, icons and Wheeler I4.ini.
   The included modified I4 DLL is required for substring matching.
3. Enable `RenamePotions_I4.esp` after `I4IconAddon.esp` and after other potion icon
   rule plugins that this patch should override. Do not rename the ESP or JSON.
4. Fully close Skyrim and start it again through SKSE. The existing poison named
   **Weapon Oil of Jarrin Crown** should match without being renamed again.

If you use Enchantment Swapper Description Fix, keep its helper DLL and scripts
enabled. This package retains its Wheeler donor-description integration; only
its bundled wheeler.dll is superseded. The standalone old Wheeler name patch
can be disabled because its changes are already included.

For a customized `SKSE/Plugins/wheeler/I4.ini`, retain these keys under `[I4]`:
`Enabled=true`, `PreferI4Icons=true`, `UseForPotions=true`, `UseForPoisons=true`.
The included SVG lets Wheeler draw the oil icon without SWF extraction.

## Wheeler and component versions

Wheeler's potion-memory fix remains included: separate batch names, counts,
activation and saved bindings are preserved. Its icon lookup uses the batch's
renamed name. A wheel slot bound before a later rename must be removed and
re-added from the renamed inventory row to bind the new name.

**v1.3 is the combined package version.** It contains:

- A modified I4 1.1.1 DLL with the new name-keyword extension.
- The exact Wheeler compatibility DLL from v1.2, unchanged, based on Refined
  1.3.3.0 and retaining the renamed-potion and Enchantment Swapper fixes.
- Updated I4 rules, the original oil artwork and an empty ESL-flagged loader ESP.

InventoryInjector.log should contain:
`InventoryInjector name-keyword compatibility v1.3 enabled`
and `Read 12 rules from RenamePotions_I4.json`.
Wheeler's startup marker will still say v1.2 because its DLL is unchanged.
These are modified compatibility builds, not official upstream releases.

## Name rule

The new rule is:

```json
{
  "match": {
    "formType": "Potion",
    "flags": ["Poison"],
    "text": {"contains": "weapon oil", "ignoreCase": true, "anyOf": []}
  },
  "assign": {
    "iconSource": "RenamePotionsI4/icons.swf",
    "iconLabel": "weapon_oil",
    "iconColor": "#E4B45D",
    "subTypeDisplay": "Weapon Oil"
  }
}
```

Keep `anyOf: []`. With the modified DLL, `contains` takes precedence. With stock
I4 accidentally winning the conflict, the empty array prevents this rule from
matching every poison. Older exact-name profiles can still match in that case,
so check the log marker when diagnosing a longer name that does not update.

The generic color is amber. Existing exact fire/frost/shock oil profiles are
evaluated afterward and retain their red-orange, blue or yellow colors.

## Validation and source

The Windows plugin is compiled from pinned upstream source and dependencies.
Automated tests cover the production substring matcher, package rules, poison
filtering, stock-I4 fallback, existing elemental overrides and icon/ESP structure.
The Wheeler DLL is checked against the previously built v1.2 hash. **Skyrim was
not available here, so this release has not been tested in game.**

To check in game, open the inventory and confirm the example poison has the oil
icon and Weapon Oil Type label. Check that an unrelated poison still shows its
normal icon, then inspect the renamed batch in Wheeler and save/reload.

`BUILD-INFO.json`, `I4-KEYWORD-BUILD-INFO.json`, `WHEELER-BUILD-INFO.json` and
`VALIDATION.json` record the components and verification. Modified source,
patches and build scripts are under `Source`; licenses are under `Licenses`.
`Source/Combined-Patch-v1.3/assemble.py` reproduces the combined archive from
the v1.2 ZIP and the new I4 component ZIP.

I4 by Parapets/Exit-9B: https://github.com/Exit-9B/InventoryInjector (MIT).
Wheeler Refined: https://github.com/c0kadam/Wheeler-Refined (GPL-3.0-only for
this modified build; original Wheeler and third-party notices retained).
Rename Potions: https://github.com/alexjiang200407/rename-potions-skse.
SkyUI: https://github.com/schlangster/skyui.
The original oil glyph remains CC0 1.0. Modification date: 2026-09-28.
