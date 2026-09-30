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
- Native poison delivery through ArrowProjectile, for player and NPC shots. The engine remains responsible for applying effects and resistance. No per-frame polling and no scripted damage simulation.
- World misses keep the poisoned ammo source; actor contact changes the recovery source to ordinary ammo before native impact processing. This includes blocked actor contacts. Recovery chance remains the engine's decision.
- Native poison-dose perks determine arrows per bottle, or use a fixed INI override. No perk records are edited.
- Static ESP/ESM, ESL, and ESL-flagged ESP sources are resolved by filename plus local FormID. The file's actual light flag determines the mask. The AE low ESL local-ID range is accepted.
- Recipes are isolated to each SKSE co-save. An ESS global fingerprint detects a missing/mismatched co-save and locks crafting instead of assigning an old item a new poison.

## Limits and compatibility

- **512 distinct recipes per save.** Identical recipes reuse a slot. Slots are never recycled, even after inventory counts reach zero: dropped ammunition and unloaded containers may still reference them. Reaching the cap cancels new recipes without consuming ingredients.
- Keep the matching `.ess` and `.skse` save files together. Do not remove/rename the companion ESP, compact its FormIDs, or uninstall it from a save containing these items.
- Start testing on a profile without the original Poisoned Arrows and Bolts mod. This beta does not migrate its DPF-generated ammunition. Existing mods can retain DPF if they independently need it; this mod neither loads nor edits DPF data.
- Missing source plugins leave their recipe slots reserved and unavailable; restore those plugins to recover functionality. Existing form IDs must not be compacted or reassigned mid-save.
- Quest items and inputs reported stolen by the engine are refused. Temporary ammo and ammunition using non-arrow projectile types are refused. Bound arrows are not a supported feature. Player-created dynamic poisons with custom per-effect CTDA conditions, VMAD data, or dynamic MGEF/keyword records are refused rather than losing that data. Normal alchemy-created poisons do not need those features.
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
