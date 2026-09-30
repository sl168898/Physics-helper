# Version 0.2.8 — Dynamic Tooltips runtime keyword correction

Addresses the logged refusal for `LoreBox_quantDTWhoseQuest` on a crafted
Weakness to Fire poison. Dynamic Tooltips creates its named keywords through
the engine form factory, without registering them in the keyword array that
0.2.7 searched. The resolver now merges that array with the global form registry
under its read lock. The original keyword is preserved by name, with the same
duplicate-name checks, and resolved again after loading. No poison effects or
tooltip keywords are discarded, and no provider-specific exception is used.

Install this full package over 0.2.7 in MO2, let its PoisonedAmmoNative.dll win,
and restart Skyrim through SKSE. No new game or animation regeneration is
needed. Keep Wheeler 1.4.0 and the existing animation setup. The reload sequence,
renamed-bottle selection, White Phial protected-bottle route, perks, and elemental
icons remain unchanged. Existing v1/v2 recipe bytes and fingerprints are unchanged.

The regression test covers the exact keyword name absent from the keyword array,
a 178% / 120-second weakness effect, mixed effects, loss of the original bottle,
and a recreated keyword with another FormID after loading. Windows compilation
and automated tests are required for this package; the user's Skyrim load order
has not been run here. Retest the same bottle, then save, restart and fire a
saved coated bolt. See TESTING.md for the focused check.

Runtime keyword providers must still be present when loading. Unnamed, missing
or ambiguous keywords, custom item scripts, dynamic magic-effect records and
per-effect conditions retain their existing checks. Rejection consumes nothing.

# Previous version 0.2.7 — runtime keyword support for crafted poisons

Fixes the remaining refusal shown as "this poison uses a temporary or unresolved
keyword" for crafted poisons carrying named runtime keywords. Keywords created
by a provider can now be saved by their unique registered EditorID and resolved
again after loading, even when their temporary FormID changes. No keyword or
poison effect is discarded. Fire-resistance reduction remains a status effect,
with its original magnitude and duration, subject to the existing coating perks.

Install this full package over 0.2.6 in MO2 and restart Skyrim through SKSE.
Keep Wheeler Elemental Ammo 1.4.0 and the existing New Anims setup. Both the
crossbow reload sequence and the crafted-poison Papyrus-handle fix are included.
No new game is needed. Existing v1 saves retain their exact data/fingerprints.
A save containing runtime-keyword coatings uses the new v2 recipe payload and
must subsequently be loaded with 0.2.7 or newer; keep the matching ESS/SKSE pair.

Inventory/Wheeler clicks still coat one bottle, and F8 opens batch selection.
Renaming a poison does not change this route. White Phial Decanting 2.1.0's
protected bottles continue to use their original persistent poison records.

A runtime keyword must have a unique EditorID, at most 260 bytes, registered in
the game's keyword array. Unnamed, missing or ambiguous keywords are refused
with the exact EditorID and reason in PoisonedAmmoNative.log. Their provider
must remain installed and recreate them before saves load. Dynamic magic-effect
records, custom item scripts and per-effect conditions remain unsupported for
crafted snapshots. Rejection consumes nothing.

The screenshot identifies the rejection category, but not the exact keyword.
This release adds the missing persistence support; it does not claim the user's
full load order has been tested in Skyrim. See TESTING.md for the targeted check.

# Version 0.2.6 — player-crafted poison correction

Fixes a false rejection of normal player-crafted poisons, including Weakness to
Fire. The previous script check used a helper which only reports whether the
item has a Papyrus handle. A handle alone does not mean the poison has attached
custom scripts or cannot be saved. The new check inspects the actual bound
script objects, allowing ordinary Potion/Form wrappers and rejecting real custom
item scripts whose state this mod cannot preserve.

Install this full package over 0.2.5 in MO2 and restart Skyrim through SKSE.
Keep Wheeler Elemental Ammo 1.4.0 and your existing New Anims setup. The 0.2.5
reload-before-poison sequence is included. Existing records, perks, recipe IDs
and co-save format are unchanged; no new game or animation regeneration is needed.

The poison's effects, magnitudes, durations, areas, costs and stable keywords are
still copied into its saved recipe. Reducing fire resistance is a supported
status effect; it is not converted into direct fire damage. Existing crafted
batches are unaffected.

If a different restriction is encountered, the notification now identifies it:
custom attached script, unsupported per-effect conditions, temporary magic effect,
temporary keyword, invalid effect list, or unstable ammunition. Rejection details
are always written to Documents/My Games/Skyrim Special Edition/SKSE/
PoisonedAmmoNative.log; TraceProjectiles need not be enabled. Reproduce once and
copy that log before restarting Skyrim if the item is still refused. The exact
user-reported bottle has not been inspected in-game, so another restriction may
still need a targeted compatibility change. Rejected requests consume nothing.

# Version 0.2.5 — finish crossbow reload before poisoning

Install this full package over 0.2.4 in MO2, let its PoisonedAmmoNative.dll win,
and restart Skyrim through SKSE. Keep Wheeler Elemental Ammo 1.4.0 installed.
Both ESPs, recipe IDs, co-save format, perks, coating quantities and elemental
icon metadata are unchanged. No new game or animation-generator run is needed.

When coating with a drawn crossbow, the bridge waits for the ammo equip and
reload to finish, allows a short transition, then requests the installed New
Anims poison animation. It delays the statistic trigger too, so the natural
poison-removal event cannot use this batch's statistic increase mid-reload.
The wait follows reload events and the IsReloading graph state, including
reloads that begin after closing Inventory or the F8 dialog. Bows and sheathed
weapons keep the previous animation route.

Switching weapon/ammo, sheathing, death or loading a save cancels a pending
crossbow visual. A newer craft replaces the older pending visual. If reload
completion cannot be established within 15 seconds of active player updates,
the visual is skipped. A cancellation does not undo crafting, refund or consume
anything else; a cancelled wait does not send its deferred statistic/animation
request. Existing animation settings and camera restrictions still apply.

Windows compilation and automated sequencing tests must pass for this package.
The exact in-game animation flow with your load order still needs the focused
checks at the top of TESTING.md. TraceProjectiles=1 logs queued/settled/cancelled
animation requests as well as the existing diagnostics.

## Previous update: 0.2.4 elemental/coating icon metadata


Use with Wheeler All Icons 2K Elemental Ammo 1.4.0. Install this full package over
0.2.3 and restart Skyrim through SKSE. Existing saves and crafted batches keep
their recipe IDs and co-save schema. Existing ESPs, perks, impact crash fix,
one-bottle click crafting, F8 batch crafting and animations are unchanged.

A read-only API reports the original ammunition element and actual coating
separately. Colors are calculated when recipes are crafted or restored from a
save. No item-name matching, scripts or polling quests are added. For mixed
recognized damage effects, color uses summed absolute magnitude x duration
(ties: fire, frost, shock, poison). Utility/status-only or unrecognized coatings
use the green generic coating marker; it does not imply poison health damage.
Native elements are read from the ammunition projectile's explosion enchantment.
Script-only elements without matching damage records may need a compatibility rule.

This update changes icon metadata only. In-game validation is still required.

# Poisoned Ammunition + Coating Perks 0.2.3 beta

Skyrim SE Steam 1.6.1170 / SKSE64 2.2.6 / Address Library / Perk Adjuster (user profile has 2.1).

## Impact crash correction and Wheeler update

Corrects a native impact-hook signature error: the AddImpact hook now returns the engine's ImpactData pointer intact after cleanup. The previous void hook could discard that pointer. This matches the invalid impact pointer in the September 30 poisoned-bolt crash log. See CRASH_FIX.md for evidence and remaining runtime checks. This is a targeted correction, not a claim that all possible crashes are resolved.

For Wheeler poison clicks, install the companion Wheeler_All_Icons_2K_Poison_Ammo_v1_3_0.zip too and let its wheeler.dll replace the previous Wheeler build. It preserves the existing 2K filtering, artwork, food/alcohol mappings and named-potion integration.

## Click a poison to coat ammunition

With a bow and ordinary arrows, or a crossbow and ordinary bolts equipped:

- **Click/use a poison in your own Inventory:** immediately consume one bottle and coat one batch. No confirmation or batch dialog. Mouse clicks and SkyUI keyboard/controller use share this route.
- **Click a poison in the updated Wheeler:** Wheeler closes and requests exactly one bottle on the game thread, without a batch dialog. Renamed poison selections are re-resolved before consuming the selected bottle. Both DLL updates are required.
- **Highlight a poison in Inventory and press F8:** open the bottle/batch dialog for 1, 5, 10 or the maximum available bottles. Opening it or selecting Cancel consumes nothing.

One batch means one bottle's dose count, including existing dose perks and Measured Dose. If fewer arrows/bolts remain, the last partial batch still consumes one bottle. Output is auto-equipped by default; equip ordinary ammunition again before making another batch. Melee poisoning, potions, food and other inventory actions retain their existing behavior.

Missing/wrong/already-coated ammunition, unavailable recipes, stolen or quest inputs, and an already-poisoned bow/crossbow produce a notification without applying poison to the weapon. Use up a previously applied weapon poison before crafting.

CraftOnPoisonUse=1 is the default in PoisonedAmmoNative.ini. Setting it to 0 and restarting restores the previous inventory-click behavior while retaining F8 crafting. This setting controls Inventory use and the updated Wheeler route. Favorites and external auto-poison mods remain outside these routes. F8 still requires a highlighted poison in Inventory.

## Immersive Interactions animation

Includes an optional native bridge for **Immersive Interactions - New Anims 1.5**, inspected from the supplied AR_Ref_AliasScript source. Keep your existing Immersive Interactions, New Anims and their dependencies installed. This ZIP does not replace their scripts, animations or settings.

After a successful batch, the bridge counts the bottles actually consumed in Skyrim's Poisons Used statistic, then notifies only the installed player alias's poison-removal handler. This handles the case where the original removal event ran before the statistic changed. The addon's previous-statistic and busy checks are retained. No extra bottle is consumed, no bow poison is applied, and no OnObjectPoisoned event is broadcast to other mods.

The animation still follows the addon's conditions: poison animations enabled, weapon drawn, actor not busy, allowed camera/first-person setup and its AR_IgnoreObject keyword. With a paused inventory it normally waits until menus close. A multi-bottle batch requests one animation, not one per bolt or bottle. Rapid crafting while another interaction is busy may skip an animation as the addon normally does. First-person bow/bolt model lookup follows the addon's own JSON mappings and remains an in-game check.

ImmersiveInteractionsBridge=1 is enabled by default; 0 disables the integration after a restart. If its player-alias script is absent, crafting still works. **The bridge is implemented against the supplied script but has not been tested inside Skyrim.**

The two existing ESPs and the Satchel mod are not changed; no Satchel ingredient-refund bridge is included.

## Three additive Marksman perks

| Perk | Requirements | Effect |
|---|---|---|
| Coating Mechanist I | Marksman 25; one perk point | Crossbow-delivered poison/oil strength x1.5 |
| Coating Mechanist II | Marksman 50 + Coating Mechanist I; one perk point | Strength x2 total; supersedes I |
| Measured Dose | Marksman 30; one perk point; no other perk prerequisite | Twice the bolts per bottle when coating through Inventory, updated Wheeler or F8 |

Measured Dose works alongside either Coating Mechanist perk. A bottle giving 5 bolts normally gives 10 with Measured Dose; with Coating Mechanist II each bolt delivers x2 strength. Existing native poison-dose perks are evaluated first. The fixed ArrowsPerBottle setting, if used, is also doubled for bolts.

## Install/update in MO2

1. Install this ZIP as an update replacing the prior Poisoned Ammunition beta, or give it file priority over that beta. Only this version of PoisonedAmmoNative.dll should win.
2. Enable both PoisonedAmmoNative.esp and CoatingMechanist.esp. Keep Perk Adjuster enabled. The existing throwing-weapon patch can remain enabled; this package adds a separate JSON file.
3. Restart Skyrim. Check the Marksman tree for the three new nodes to the right of the existing tree. Rank I and rank II are connected; Measured Dose is independently available at 30.
4. Equip a crossbow and ordinary bolts. Click the poison/oil in Inventory for one bottle immediately, or highlight it and press F8 for bulk coating. Measured Dose doubles bolts per bottle in both modes.

The original ammo/proxy form IDs, 512 recipe slots, co-save version and companion ammo ESP are unchanged. Keep the matching ESS + SKSE co-save. Measured Dose is a crafting bonus: it does not retroactively add bolts to existing stacks. Coating Mechanist is checked on the shooter at impact and also benefits already-crafted bolts. Bows/arrows and melee coatings receive neither crossbow bonus.

## What strength means

The native plugin multiplies the delivered active effect's magnitude. For effects explicitly flagged No Magnitude, it multiplies their duration instead. It does not multiply both magnitude and duration, change resistance/immunity, increase physical bolt damage, or modify shared bottle/MGEF records. Zero-value marker effects are left alone. Native poison-slot weapon oils are treated the same as poison regardless of their names. Oils whose damage is hardcoded inside a separate script, or whose delivery bypasses the native projectile-poison path, require a specific compatibility patch and are not claimed supported here.

The perk records contain purchase requirements; their mechanics run in the supplied DLL. No existing perk or AVIF is overridden. Perk Adjuster adds positions/connections at runtime. Visual overlap with other added branches must be checked in the user's full load order.

## Validation and limitations

The inventory callback adapter has been built against the pinned CommonLib API but requires the in-game input/animation checks in TESTING.md. Automated tests cover every combination of the three perks, non-stacking rank II, doubled bolt counts (not arrows), partial batches, finite scaling and unchanged persistence. The DLL must also pass the Windows build. The production impact wrapper is compiled in a regression test checking pointer and null returns, all forwarded arguments and scope cleanup. This beta has not been tested inside Skyrim. The separate earlier NPC spell-reapplication crash is not established as the same fault. Use a copied save for the checks in TESTING.md.

TraceProjectiles=1 in SKSE/Plugins/PoisonedAmmoNative.ini logs dose decisions, coating impact context and each adjusted effect to Documents/My Games/Skyrim Special Edition/SKSE/PoisonedAmmoNative.log. Copy the log before restarting, because each launch overwrites it.

## Previous ammunition functionality

# Poisoned Ammunition SKSE — 0.1.0 beta

An independent native implementation inspired by shazdeh2's **Poisoned Arrows and Bolts** (Nexus 123585). No code or assets from that mod or Dynamic Persistent Forms are included.

**Target: Steam Skyrim SE/AE 1.6.1170, SKSE64 2.2.6, and the matching Address Library.** This build refuses other runtimes. Install the DLL and its ESL-flagged ESP together. Core ammunition crafting needs no added Papyrus scripts, DPF, SPID, B612 UI, MCM Helper, PapyrusUtil or Papyrus Extender. The optional animation integration uses the already-installed Immersive Interactions scripts and their own dependencies.

## Status

This is a compiled beta, **not an in-game-validated release**. Automated tests cover save encoding, ESL form identity, malformed data, recipe capacity and crafting arithmetic. The Windows compiler verifies the CommonLib interface. Only running Skyrim can verify projectile hook timing, follower use, native resistance, recovery and ongoing effects across saves. Use the included TESTING.md on a separate MO2 profile/copied save before adopting it in a playthrough.

## Use

1. Equip an ordinary bow and arrows, or crossbow and bolts. Use up any poison already applied to the weapon first.
2. Open your own inventory and **use/click a poison** for one bottle immediately.
3. For bulk crafting, highlight the poison and press **F8** (configurable), then choose how many bottles to use. The dialog shows the output count. Cancel consumes nothing. A partially used final bottle is consumed.
4. The separate named ammunition stack is equipped automatically by default. You can transfer it, put it in containers, drop it, or select it in Wheeler like other ammunition.

Inventory poison use immediately coats one bottle's batch; the hotkey opens bulk selection. Melee and other item use retain their normal behavior. No new MCM, SWF replacement, or interface framework dependency is added. The dialog uses normal messagebox controls.

## Implemented behavior

- Separate arrow/bolt stacks for each ammunition and poison combination. Original damage, projectile, model/texture swaps, weight, value, sounds and keywords are copied from the resolved source ammunition.
- Static poisons are used directly, retaining their effects and conditions. Standard player-crafted poisons are snapshotted with effect magnitudes, areas, durations, costs and keywords; the original temporary potion form need not survive.
- Native poison delivery through ArrowProjectile, for player and NPC shots. The engine remains responsible for applying effects and resistance. Projectile delivery uses hooks rather than polling or scripted damage simulation.
- World misses keep the poisoned ammo source; actor contact changes the recovery source to ordinary ammo before native impact processing. This includes blocked actor contacts. Recovery chance remains the engine's decision.
- Native poison-dose perks determine arrows per bottle, or use a fixed INI override. No perk records are edited.
- Static ESP/ESM, ESL, and ESL-flagged ESP sources are resolved by filename plus local FormID. The file's actual light flag determines the mask. The AE low ESL local-ID range is accepted.
- Recipes are isolated to each SKSE co-save. An ESS global fingerprint detects a missing/mismatched co-save and locks crafting instead of assigning an old item a new poison.

## Limits and compatibility

- **512 distinct recipes per save.** Identical recipes reuse a slot. Slots are never recycled, even after inventory counts reach zero: dropped ammunition and unloaded containers may still reference them. Reaching the cap cancels new recipes without consuming ingredients.
- Keep the matching `.ess` and `.skse` save files together. Do not remove/rename the companion ESP, compact its FormIDs, or uninstall it from a save containing these items.
- Start testing on a profile without the original Poisoned Arrows and Bolts mod. This beta does not migrate its DPF-generated ammunition. Existing mods can retain DPF if they independently need it; this mod neither loads nor edits DPF data.
- Missing source plugins leave their recipe slots reserved and unavailable; restore those plugins to recover functionality. Existing form IDs must not be compacted or reassigned mid-save.
- Quest items and inputs reported stolen by the engine are refused. Temporary ammo and ammunition using non-arrow projectile types are refused. Bound arrows are not a supported feature. Player-created dynamic poisons with custom per-effect CTDA conditions, attached custom item scripts, dynamic MGEF records, or runtime keywords without a unique registered EditorID are refused rather than losing that data. Normal alchemy-created poisons do not need those features.
- Poisoning a bow already holding another poison is refused during crafting. If another mod later puts weapon poison on a bow firing this ammo, the ammo's poison takes precedence; the engine may still consume the weapon's charge. Avoid combining both.
- NPC ammunition consumption follows Skyrim and your installed mods, including vanilla infinite follower ammunition behavior. This mod does not change AI consumption rules.
- Requiem/LoreRim compatibility is a design target, **not a tested certification**. Original keywords and native perk calculations are retained, but mods that match exact ammo FormIDs, replace projectile hooks without chaining, consume items in event callbacks, or cache base-form data may need a patch.
- Wheeler needs no special integration to select these stable ammo records. It uses the underlying ammo model/keywords; this package adds no icons.
- Changing source poison records changes already-created batches using that static poison. Custom player-created poisons retain their recorded effect values.

## Install and diagnose

Install the ZIP in MO2. Enable **PoisonedAmmoNative.esp** in the right pane and launch through SKSE. The ESP has only new template forms and a save marker; it contains no leveled-list or world edits.

Settings: `Data/SKSE/Plugins/PoisonedAmmoNative.ini`; restart Skyrim after changes.

Log: `Documents/My Games/Skyrim Special Edition/SKSE/PoisonedAmmoNative.log` (or your configured equivalent). Expect `Ready: 512 stable ESL slots`. Set `TraceProjectiles=1` for troubleshooting and attach the log with a description of the shot/recovery result.

All source, the deterministic ESP generator, tests, pinned build script, and third-party licenses are included. BuildInfo.json records the exact source revision and DLL hash. See DESIGN.md for implementation details.
