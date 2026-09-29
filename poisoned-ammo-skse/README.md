# Poisoned Ammunition SKSE — 0.1.0 beta

An independent native implementation inspired by shazdeh2's **Poisoned Arrows and Bolts** (Nexus 123585). No code or assets from that mod or Dynamic Persistent Forms are included.

**Target: Steam Skyrim SE/AE 1.6.1170, SKSE64 2.2.6, and the matching Address Library.** This build refuses other runtimes. Install the DLL and its ESL-flagged ESP together. No Papyrus scripts, DPF, SPID, B612 UI, MCM Helper, PapyrusUtil, or Papyrus Extender are required by this mod.

## Status

This is a compiled beta, **not an in-game-validated release**. Automated tests cover save encoding, ESL form identity, malformed data, recipe capacity and crafting arithmetic. The Windows compiler verifies the CommonLib interface. Only running Skyrim can verify projectile hook timing, follower use, native resistance, recovery and ongoing effects across saves. Use the included TESTING.md on a separate MO2 profile/copied save before adopting it in a playthrough.

## Use

1. Equip an ordinary bow and arrows, or crossbow and bolts. Use up any poison already applied to the weapon first.
2. Open your own inventory and highlight a poison. Press **F8** (configurable).
3. Choose how many bottles to use. The dialog shows exactly how many arrows/bolts that produces. Cancel consumes nothing. A partially used final bottle is consumed.
4. The separate named ammunition stack is equipped automatically by default. You can transfer it, put it in containers, drop it, or select it in Wheeler like other ammunition.

The interface deliberately uses a dedicated hotkey. Ordinary clicking a poison retains your existing poison behavior. There is no new MCM or interface framework dependency. Keyboard input is required to open the batch dialog; its buttons use the normal messagebox controls.

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
- Quest items and inputs reported stolen by the engine are refused. Temporary ammo is refused. Bound arrows are not a supported feature. Player-created dynamic poisons with custom per-effect CTDA conditions, VMAD data, or dynamic MGEF/keyword records are refused rather than losing that data. Normal alchemy-created poisons do not need those features.
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
