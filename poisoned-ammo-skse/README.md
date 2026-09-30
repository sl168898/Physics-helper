# Poisoned Ammunition + Coating Perks 0.2.1 beta

Skyrim SE Steam 1.6.1170 / SKSE64 2.2.6 / Address Library / Perk Adjuster (user profile has 2.1).

## Click a poison to coat ammunition

With a bow and ordinary arrows, or a crossbow and ordinary bolts equipped, use a poison in your own Inventory to open the bottle/batch dialog. Mouse clicks and SkyUI keyboard/controller item-use actions share this route. Choose the batch size or Cancel; opening/cancelling the dialog consumes nothing. F8 remains an alternative. Melee poisoning, potions, food and other inventory actions retain their existing behavior.

Missing/wrong/already-coated ammunition, unavailable recipes, stolen or quest inputs, and an already-poisoned bow/crossbow produce the existing validation message. These failures do not fall back to applying poison to the weapon. Use up a previously applied weapon poison before crafting.

CraftOnPoisonUse=1 is the default in PoisonedAmmoNative.ini. Setting it to 0 and restarting restores the previous inventory-click behavior while retaining F8 crafting. This option concerns the player's Inventory menu; external auto-poison mods, Favorites and direct Wheeler poison use are not rerouted.

**Animation compatibility is not verified.** This route bypasses the normal weapon-poisoning action. An animation mod listening for that action may not play. This build does not replay a weapon-poisoning event or consume an extra bottle to trigger an animation. A verified animation bridge requires the actual installed animation mod/scripts. The two existing ESPs and the Satchel mod are not changed; no Satchel ingredient-refund bridge is included.

## Three additive Marksman perks

| Perk | Requirements | Effect |
|---|---|---|
| Coating Mechanist I | Marksman 25; one perk point | Crossbow-delivered poison/oil strength x1.5 |
| Coating Mechanist II | Marksman 50 + Coating Mechanist I; one perk point | Strength x2 total; supersedes I |
| Measured Dose | Marksman 30; one perk point; no other perk prerequisite | Twice the bolts per bottle when coating through Inventory use or F8 |

Measured Dose works alongside either Coating Mechanist perk. A bottle giving 5 bolts normally gives 10 with Measured Dose; with Coating Mechanist II each bolt delivers x2 strength. Existing native poison-dose perks are evaluated first. The fixed ArrowsPerBottle setting, if used, is also doubled for bolts.

## Install/update in MO2

1. Install this ZIP as an update replacing the prior Poisoned Ammunition beta, or give it file priority over that beta. Only this version of PoisonedAmmoNative.dll should win.
2. Enable both PoisonedAmmoNative.esp and CoatingMechanist.esp. Keep Perk Adjuster enabled. The existing throwing-weapon patch can remain enabled; this package adds a separate JSON file.
3. Restart Skyrim. Check the Marksman tree for the three new nodes to the right of the existing tree. Rank I and rank II are connected; Measured Dose is independently available at 30.
4. To coat a batch, equip a crossbow and ordinary bolts, click the poison/oil in Inventory (or highlight it and press F8). The bottle/bolt dialog shows the doubled count when Measured Dose is owned.

The original ammo/proxy form IDs, 512 recipe slots, co-save version and companion ammo ESP are unchanged. Keep the matching ESS + SKSE co-save. Measured Dose is a crafting bonus: it does not retroactively add bolts to existing stacks. Coating Mechanist is checked on the shooter at impact and also benefits already-crafted bolts. Bows/arrows and melee coatings receive neither crossbow bonus.

## What strength means

The native plugin multiplies the delivered active effect's magnitude. For effects explicitly flagged No Magnitude, it multiplies their duration instead. It does not multiply both magnitude and duration, change resistance/immunity, increase physical bolt damage, or modify shared bottle/MGEF records. Zero-value marker effects are left alone. Native poison-slot weapon oils are treated the same as poison regardless of their names. Oils whose damage is hardcoded inside a separate script, or whose delivery bypasses the native projectile-poison path, require a specific compatibility patch and are not claimed supported here.

The perk records contain purchase requirements; their mechanics run in the supplied DLL. No existing perk or AVIF is overridden. Perk Adjuster adds positions/connections at runtime. Visual overlap with other added branches must be checked in the user's full load order.

## Validation and limitations

The inventory callback adapter has been built against the pinned CommonLib API but requires the in-game input/animation checks in TESTING.md. Automated tests cover every combination of the three perks, non-stacking rank II, doubled bolt counts (not arrows), partial batches, finite scaling and unchanged persistence. The DLL must also pass the Windows build. This beta has not been tested inside Skyrim. The previously reported NPC spell-reapplication crash remains unresolved; this update is not a crash fix. Use a copied save for the checks in TESTING.md.

TraceProjectiles=1 in SKSE/Plugins/PoisonedAmmoNative.ini logs dose decisions, coating impact context and each adjusted effect to Documents/My Games/Skyrim Special Edition/SKSE/PoisonedAmmoNative.log. Copy the log before restarting, because each launch overwrites it.

## Previous ammunition functionality

# Poisoned Ammunition SKSE — 0.1.0 beta

An independent native implementation inspired by shazdeh2's **Poisoned Arrows and Bolts** (Nexus 123585). No code or assets from that mod or Dynamic Persistent Forms are included.

**Target: Steam Skyrim SE/AE 1.6.1170, SKSE64 2.2.6, and the matching Address Library.** This build refuses other runtimes. Install the DLL and its ESL-flagged ESP together. No Papyrus scripts, DPF, SPID, B612 UI, MCM Helper, PapyrusUtil, or Papyrus Extender are required by this mod.

## Status

This is a compiled beta, **not an in-game-validated release**. Automated tests cover save encoding, ESL form identity, malformed data, recipe capacity and crafting arithmetic. The Windows compiler verifies the CommonLib interface. Only running Skyrim can verify projectile hook timing, follower use, native resistance, recovery and ongoing effects across saves. Use the included TESTING.md on a separate MO2 profile/copied save before adopting it in a playthrough.

## Use

1. Equip an ordinary bow and arrows, or crossbow and bolts. Use up any poison already applied to the weapon first.
2. Open your own inventory and **use/click a poison**, or highlight it and press **F8** (configurable).
3. Choose how many bottles to use. The dialog shows exactly how many arrows/bolts that produces. Cancel consumes nothing. A partially used final bottle is consumed.
4. The separate named ammunition stack is equipped automatically by default. You can transfer it, put it in containers, drop it, or select it in Wheeler like other ammunition.

Inventory poison use now opens the same batch dialog as the optional hotkey when a bow/crossbow is equipped. Melee and other item use retain their normal behavior. No new MCM, SWF replacement, or interface framework dependency is added. The dialog uses normal messagebox controls.

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
