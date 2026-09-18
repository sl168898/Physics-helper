# Runemaster Enchanted Weapons 1.1.0

Use Runemaster runes alongside normal weapon enchantments. Version 1.1 also
makes all eight permanent rune weapons enchantable and preserves player-made
enchantments when an ebony weapon is converted at the forge.

Requires Skyrim SE **1.6.1170**, SKSE64, the matching Address Library, and
Kittytail's Runemaster Magic **1.5** (`RunemasterMagic.esl`). Windows x64 only.

## Install or update in Mod Organizer 2

1. Let active castable Runemaster weapon runes expire on yourself and nearby
   companions. Unequip permanent rune weapons, save and quit the game.
2. Install this ZIP. If updating from 1.0, **replace the previous add-on**.
   Keep the original Runemaster mod enabled. This is the complete updated mod;
   no separate hotfix or extra dependency is required.
3. Enable `Runemaster Enchanted Weapons.esp` after Runemaster and your
   Runemaster/LoreRim balance patches. It is ESL flagged, with sixteen hidden
   impact spells. The original eight spell IDs remain unchanged from 1.0.
4. Launch through SKSE. Existing permanent rune weapons are supported; you do
   not need to obtain new copies. No new power, inventory item or hotkey is added.

## Permanent rune weapons (new in 1.1)

The Sundering Runic Battleaxe, Almighty Runic Bow, Spectral Runic Dagger,
Umbral Runic Greatsword, Baleful Runic Mace, Tempest Runic Sword, Stunning Runic
War Axe and Kinetic Runic Warhammer can receive ordinary player enchantments
at an arcane enchanter. Their permanent runes remain attached to the weapon type.

- The permanent rune activates from that weapon's physical hits, including
  arrows from the Almighty Runic Bow. Using another weapon does not trigger it.
- Original rune effects, scripts and shared cooldown spells remain in use.
  A rune does not require soul charge. An added normal enchantment consumes
  charge as usual and can be recharged normally.
- Converting a player-enchanted ebony weapon through its original rune recipe
  preserves the enchantment definition/strength, maximum and remaining charge,
  player-set name and tempering of the actual consumed inventory copy.
  An unenchanted input still produces the usual unenchanted runic weapon.
- The winning recipe still decides which ingredients are accepted and which
  perks are required. This does not make every separately defined enchanted
  loot weapon an eligible ingredient, or add alternative recipes.
- Weapon stats, meshes, crafting costs and perk requirements are unchanged.
  A different base enchantment added by another patch is left intact; ordinary
  Skyrim enchanting restrictions still apply to that other enchantment.

The DLL removes the original rune enchantment and its enchanting prohibition
from the eight loaded weapon records, then handles rune hits separately. The
original plugin on disk is not edited. Their original rune description may
therefore no longer appear in the weapon's ordinary enchantment panel.

## Castable runes (included from 1.0)

- Supports Stunning Blast, Almighty Bolt, Runestorm, Sundering Inferno, Havoc,
  Spectral Slash, Umbral Veil and Baleful Glow, including Transfer Rune on
  companions. One-handed and two-handed strikes are supported; either hand
  can trigger the active rune when dual wielding.
- Normal weapon enchantments and charge usage remain handled by Skyrim.
  The castable rune is a separate timed effect, with no soul charge of its own.
- Original powers, costs, rune items, conditions, perks and scripts remain in
  use. The DLL reads the winning loaded records rather than overriding your
  balance patches with copies of the original records.
- Almighty Bolt, Runestorm, Sundering Inferno, Havoc and Umbral Veil consume the
  castable rune on a hit. Stunning Blast, Spectral Slash and Baleful Glow remain
  for their original duration. No Recast restrictions remain in use.
- A new castable rune replaces the previous one. Spell hits, staff attacks,
  unarmed attacks and bashes do not trigger rune impacts.
- Original impact and explosion visuals remain. The temporary weapon glow
  may differ because a replacement weapon enchantment is no longer installed.

## Validation and first in-game checks

The Windows DLL build and automated rune/crafting checks must pass before
packaging. **This release has not been tested inside Skyrim.**

1. Enchant a permanent rune weapon at an arcane enchanter, then hit an enemy.
   Check that both effects work and that subsequent rune activations respect
   the cooldown. Recharge the normal enchantment as usual.
2. Convert a player-enchanted ebony weapon at the forge. Check its enchantment
   and remaining charge, then save/reload and check again. For a clear first
   test, carry just one eligible ebony weapon.
3. Cast a temporary rune on an ordinarily enchanted weapon and check that both
   effects still work. Test a one-use rune and a sustained rune.

`RunemasterEnchantmentBridge.log` is in your SKSE log directory. Successful
startup reports eight castable runes and eight enchantable permanent rune
weapons. Successful crafting transfer logs `Preserved crafted enchantment`.
Unexpected or ambiguous inventory changes are logged and left untouched.
There are no on-screen notifications. The first 24 rune impacts per session
are logged for diagnosis.

Keep a pre-installation save while evaluating the build. To remove the add-on,
return to that save: normal enchantments placed on permanent rune weapons now
depend on the add-on separating the rune from the normal enchantment slot.

## Implementation and credits

Sixteen hidden carrier spells reference the original rune effects. The DLL
copies their winning effect conditions, preserves the original Papyrus proc
scripts, and applies physical-hit effects on the game task queue. Permanent
weapon cooldowns use the original spells. Temporary runes retain their original
cost scripts and duration. Source weapon/output inventory snapshots plus the
crafting event identify the consumed enchantment and the newly crafted copy;
no extra weapon is added or removed by the transfer.

Runemaster Magic and all rune content are by Kittytail:
https://www.nexusmods.com/skyrimspecialedition/mods/145420
This add-on contains no Runemaster artwork, meshes, sounds, textures or scripts.
CommonLibSSE-NG is used under its included license.
Source: https://github.com/sl168898/Physics-helper/tree/codex/runemaster-enchanted-weapons/runemaster-enchanted-weapons
