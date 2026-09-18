# Runemaster Enchanted Weapons 1.0.0

Adds Runemaster's rune hit effects alongside an existing weapon enchantment.
For Skyrim SE 1.6.1170, SKSE64, the matching Address Library, and Kittytail's
Runemaster Magic 1.5 (`RunemasterMagic.esl`). Windows x64 only.

## Installation in Mod Organizer 2

1. Before enabling this add-on, let all active Runemaster weapon runes expire
   on yourself and nearby companions. In the original mod they last 30 seconds;
   if your balance patch extends them, wait until those effects have ended.
   Save and quit. This avoids changing the native effect type of a saved rune
   while it is active.
2. Install this ZIP as a new mod. Keep the original Runemaster mod enabled.
3. Enable `Runemaster Enchanted Weapons.esp` in the right pane and place it
   after Runemaster and your Runemaster/LoreRim balance patches. It is ESL
   flagged and contains only eight new hidden impact spells.
4. Launch through SKSE. Cast a rune normally while wielding an enchanted
   weapon, then hit an enemy. No new power, item, menu or hotkey is added.

## Behavior

- Supports Stunning Blast, Almighty Bolt, Runestorm, Sundering Inferno,
  Havoc, Spectral Slash, Umbral Veil and Baleful Glow, including Transfer Rune
  on companions. One-handed and two-handed weapon strikes use the same route;
  either hand can trigger the active rune when dual wielding.
- Your weapon's normal enchantment and charge usage are handled by Skyrim as
  usual. The rune is a separate timed effect; it does not alter, replace or
  save a new enchantment onto the weapon. It needs no soul charge of its own.
- Original powers, costs, rune items, perk requirements, scripts and damage
  effects remain in use. The DLL uses the winning loaded rune records, so
  an ESP here does not overwrite your existing balance/perk integration.
- Almighty Bolt, Runestorm, Sundering Inferno, Havoc and Umbral Veil consume
  the rune on a hit. Stunning Blast, Spectral Slash and Baleful Glow stay active
  for their original duration. The original effect's No Recast restriction
  prevents duplicate impacts on an already affected target.
- Casting another rune replaces the previous active rune. Runes do not stack.
  A one-use rune is consumed by the first eligible hit across either hand.
- Unenchanted weapons still work. Spell hits, staff attacks, unarmed attacks
  and bashes cannot trigger rune impacts. Existing casting/equipment conditions
  are retained; this does not broaden which weapon types the powers accept.
- No changes to Runemaster's permanent runic weapons or to the enchanting tree.
- Original impact/explosion visuals remain in use. The temporary weapon glow
  may differ because the replacement weapon enchantment is no longer installed.

## Validation and first in-game check

Windows compilation and automated selection/filtering checks are provided.
This release has NOT been tested inside Skyrim; treat it as a test build.

1. Try a normally enchanted weapon and a player-enchanted weapon. Confirm the
   ordinary enchantment still activates and still consumes charge as usual.
2. Apply Stunning Blast and strike a target: confirm both effects activate.
3. Try Almighty Bolt: confirm its original delayed impact happens once and
   the rune ends. Try another hit without recasting: no second rune impact.
4. Let a sustained rune expire, and test dual wielding/two-handed weapons or
   Transfer Rune if you use those features. Save/reload with a newly cast rune
   to verify its remaining duration is retained.

The SKSE log directory contains `RunemasterEnchantmentBridge.log`. A successful
initialization reports `Ready: eight additive runes...`. The first 24 impacts
per session are logged for diagnosis; no on-screen notifications are shown.
If forms are missing or a different mod fundamentally changes the rune
implementation, the DLL logs a reason and leaves the original system intact.

Before disabling this add-on, let active runes expire again and save. Keep a
save made before installation while evaluating the test build.

## Implementation / credits

At DataLoaded, the DLL validates all eight runes and their transfer variants,
copies the winning enchantment impact effects/conditions into eight carrier
spells, and changes only the rune buffs' EnhanceWeapon scaffolding to Script.
The original cost scripts still execute. Physical TESHitEvent notifications
apply the carriers on the game task queue using safe actor handles. It does
not detour the combat damage function or edit inventory enchantment data.

Runemaster Magic and all rune content are by Kittytail:
https://www.nexusmods.com/skyrimspecialedition/mods/145420
This add-on contains no Runemaster artwork, meshes, sounds, textures or scripts.
CommonLibSSE-NG is used under its included license.
Source: https://github.com/sl168898/Physics-helper/tree/codex/runemaster-enchanted-weapons/runemaster-enchanted-weapons
