# Alchemical Precision 0.3.0 beta: in-game acceptance check

1. Install the complete archive and launch Steam 1.6.1170 through SKSE. In the
   Marksman tree, find Alchemical Precision connected above Measured Dose.
   It must require Measured Dose, base Marksman 60, and one perk point. Check
   Marksman 59, then 60; test without and with Measured Dose. Do not use console
   addperk to evaluate purchase requirements, because it bypasses the perk menu.
2. Enable `TraceProjectiles=1` in `SKSE/Plugins/PoisonedAmmoNative.ini`, restart,
   and inspect `Documents/My Games/Skyrim Special Edition/SKSE/PoisonedAmmoNative.log`.
   Expect `Alchemical Precision ready` at startup. A fired coated bolt with the
   perk must log `Alchemical Precision shot` and the firing Stamina.
3. Fire plain bolts, coated arrows from a bow, and coated bolts without the perk.
   None should log an enhanced chance/critical bonus. With the perk, test coated
   crossbow bolts separately with poison, elemental oil, and pure weakness oil.
   Coating Mechanist I/II must not be needed.
4. With existing critical perks active, inspect `Alchemical Precision chance`:
   enhanced chance must be ordinary +25, capped at 100. The engine rolls once;
   a noncritical shot must not receive the critical-bonus multiplier. On a native
   critical, the `critical bonus` line must be ordinary * (1 + 0.02 * firing Stamina).
   For ordinary=20, Stamina 100/500/1000 produces 60/220/420. These log values are
   the native critical bonus, not the full hit or the target's final Health loss.
5. Fire at a distant target, then change Stamina or switch weapons before impact.
   The bonus must use the logged firing Stamina and source crossbow. Test blocked
   hits, armored targets, misses, rapid consecutive bolts and a reflected-damage
   target. There must be no multiplier applied to poison ticks or reflected damage.
6. Save/reload with coated ammunition in inventory, then fire a fresh bolt. The
   coating, new perk and critical behavior must still work. Snapshots of bolts
   already in flight are intentionally cleared on load; no impact-time Stamina
   substitute is used. Turn tracing back off when finished.

Automated checks cover native modifier ordering, exactly one original entry
call, nested-query isolation, real Windows x64 variadic forwarding and return
registers, Stamina arithmetic, shot-cache lifetime, the production impact pointer
ABI, generated perk records/tree prerequisites, and all existing recipe tests.
They do not substitute for this in-game check.

# Coating Mechanist balance checks, v0.2.9

1. Confirm the startup log says 0.2.9 and the perk descriptions say 25% / 50%.
2. Against the same target with the same poison and bolt, compare no perk,
   rank I, and both ranks. A magnitude of 100 becomes 100 / 125 / 150 before
   unchanged downstream resistance calculations. Rank II alone also uses x1.5.
3. For an effect flagged No Magnitude, a duration of 8 becomes 8 / 10 / 12;
   magnitude-bearing effects keep their original duration.
4. Previously coated bolts must use the current rank at impact. Measured Dose
   still doubles the bolt quantity and does not alter effect strength.

The existing portable perk-combination and scaling checks cover these values.
Skyrim itself was not run here; verify the above in game after installing.

# Global form keyword checks, v0.2.8 (not yet run in Skyrim)

1. Confirm the startup log says 0.2.8. Keep Dynamic Tooltips enabled. Use the
   same renamed Weapon Oil of Fire Weakness (178% for 120 seconds) that failed
   with `LoreBox_quantDTWhoseQuest`. Coat ordinary bolts in Inventory, then test
   Wheeler and F8. A successful one-bottle click consumes exactly one bottle.
2. Verify the original weakness effect on impact, subject to existing resistance
   and coating perks. Test both a remaining stack and its last bottle.
3. Save with coated bolts, quit Skyrim, restart and load the matching ESS/SKSE
   pair. Fire a saved bolt and verify the effect again. Loading must not report
   an unavailable runtime keyword or an ESS/co-save mismatch.
4. Check a renamed poison and a White Phial Decanting 2.1.0 protected bottle.
   Repeat the drawn-crossbow sequence in first and third person: reload, then
   the installed poison animation. These routes are unchanged by this update.

The production resolver test includes a factory-created keyword in the global
FormID map but absent from TESDataHandler's array. It verifies the reported
EditorID, original effects, changed pointer/FormID after loading, case folding,
irrelevant non-keyword forms, null entries, duplicate pointers across registries,
ambiguous names both within and across registries, unavailable global map,
released read locks, and retry after a temporarily unavailable registry.
Running the new regression against the delivered 0.2.7 resolver fails; it passes
with the corrected resolver. Engine doubles do not replace the in-game check.

# Previous runtime keyword checks, v0.2.7 (not yet run in Skyrim)

1. Use the exact Weapon Oil of Fire Weakness from the screenshot (139% for
   120 seconds), then the separate 178% stack. With matching ranged equipment,
   click each through Inventory/Wheeler and test F8 batch coating. Every result
   must retain the selected poison's strength/duration and existing dose perks.
2. Save with coated arrows/bolts, quit to desktop, restart through SKSE and load.
   Confirm the saved ammunition retains its effects. Test a poison with both
   normal plugin keywords and generated provider keywords, and its last bottle.
3. Load an existing 0.2.6 save with coated ammunition: existing recipe slots and
   effects must remain available without an ESS/co-save mismatch warning.
4. Test renamed bottles and White Phial decanted crafted poisons as before.
5. On any remaining refusal, copy the exact notification and the current
   PoisonedAmmoNative.log before restarting. It now records the runtime
   keyword's EditorID and whether it is absent, ambiguous or unregistered.

Automated tests compile the actual PoisonSnapshot.h and PoisonKeywords.h using
engine doubles. They cover a fire-weakness fixture with mixed static/generated
keywords, changed FF IDs and object addresses after reload, mixed-case names,
duplicate registrations of the same object, missing/ambiguous names, different
objects using the same name, unavailable keyword registry, maximum names, and
White Phial's static-record route. No engine pointer or FF keyword ID is saved.
Core tests compare v1 bytes and fingerprints with a golden fixture generated by
delivered 0.2.6, exercise v2 mixed recipes and maximum valid saves, and run 6000
checksum-valid parser mutations. Real Skyrim integration remains an in-game test.

# Player-crafted poison checks, v0.2.6 (not yet run in Skyrim)

1. Use the same player-crafted Weakness to Fire poison that was rejected.
   With a crossbow and ordinary bolts, click it in Inventory: consume one bottle,
   create one batch with its original effects, then reload before poisoning.
2. Repeat with F8 and Wheeler, then a bow/arrows. Test a mixed-effect crafted
   poison and the LAST bottle of a crafted poison. The output must preserve all
   original effect magnitudes/durations, subject to the existing coating perks.
3. Save, quit to desktop, restart and load with the matching co-save. Fire a
   saved coated bolt and verify the target's fire-resistance reduction/duration.
   Native resistance/immunity remains in effect; this is not direct fire damage.
4. Confirm bought/found poisons, elemental icons, dose perks, and first/third
   person reload sequencing still work. Old stable recipe records are unchanged.
5. If refused, capture the new exact notification and PoisonedAmmoNative.log
   from that session. The log identifies the rejected poison and component;
   its component index is zero-based. No items may be consumed on rejection.

The production snapshot adapter is compiled into tests using engine doubles.
Tests cover a nonempty VM handle without scripts (the original failure), native
Potion/Form wrappers, actual custom scripts, unknown script metadata, missing
VM/policy, unrelated script handles, static scripted records, mixed effects,
last-bottle independence, byte-format-compatible save/load, and every retained
component rejection. Windows compilation validates the real pinned VM API.

# Crossbow reload sequencing checks, v0.2.5 (not yet run in Skyrim)

1. Draw your Iron Heavy Crossbow with ordinary Iron Bolts. Click a poison in
   Inventory, wait several seconds in the menu, then close it. Expect one full
   reload, then one poison animation, with no resumed reload afterward.
2. Repeat through Wheeler and the F8 batch dialog; test ordinary and Dwarven
   crossbows, standing/sneaking, Quick Shot or other reload-speed perks, and
   your installed crossbow animation replacer.
3. Repeat in third person and your supported first-person New Anims setup.
   Test Skyrim Souls if installed. Inventory/dialog time must not bypass the wait.
4. During the pending reload, switch weapons/ammo, sheathe, or load another
   save. The old poison visual must not play later. Crafting already committed
   once; no additional bottles or bolts may be consumed or refunded.
5. Test the last bottle of a player-made poison, a 5-bottle F8 batch, and
   AutoEquip=0. A valid completed batch requests at most one animation; a normal
   completed 5-bottle animation request increments Poisons Used by five.
6. Test bows and disabled/missing Immersive Interactions. Keep the previous
   behavior. Confirm the Wheeler elemental icons and impact crash correction
   still work with a newly crafted and an existing saved bolt stack.

Automated coverage uses the production ReloadGate with fast/slow reloads,
menu pauses, delayed equip, event-only and graph-only completion, restart during
settling, cancellation, missing events/state, timeout, and exactly-once dispatch.
The Windows build compiles the actual hooks against pinned CommonLibSSE-NG.

# Elemental icon checks, v0.2.4

- Use Wheeler 1.4.0 with this DLL. Check normal and fire/frost/shock ammo before coating.
- Coat normal ammo with each of the four damage types: tip and drop should match.
- Coat a fire bolt with a health-damage poison: orange tip, green drop.
- Coat frost ammo with fire oil: blue tip, orange drop.
- Load existing batches, reload the save, then switch characters with different
  recipes assigned to the same stable slot: colors must follow each recipe.
- A weakness-to-fire poison alone remains a generic green coating, not fire damage.
- Test impact, recovery and one-bottle/F8 crafting as in the retained checks below.

# 0.2.3 impact and Wheeler checks (not yet run in Skyrim)

- Reproduce the reported Iron Heavy Crossbow + poisoned Iron Bolt shot at an actor with the same load order. Test Coating Mechanist absent, rank I and rank II. Confirm no impact crash and one poison application.
- Repeat with ordinary/unpoisoned bolts and arrows, poison arrows, actor hits, blocked hits, world hits and misses. The corrected return ABI applies to all arrows/bolts passing through the hook.
- Keep the user's existing Core Impact Framework, Sanguine Symphony and Splashes of Skyrim enabled for this retest. The crash log alone did not establish a fault in those plugins.
- Test an existing poisoned-bolt stack from the prior save, then a newly made stack. The static record IDs and co-save format are unchanged.
- With both supplied DLLs winning in MO2, click a poison in Wheeler with crossbow + ordinary bolts equipped: close wheel, one bottle consumed, one batch, no dialog, no poison on the crossbow. Test both Wheeler use buttons and Measured Dose on/off.
- Test two differently renamed poison stacks with the same base form: use the chosen name only. Remove that named stack before the queued action: refuse without consuming the other name.
- Change weapon/ammo while the wheel closes or load another save: refuse the stale request. Missing/wrong/coated ammo must not fall back to weapon poisoning.
- Wheeler melee poison use stays native. Disabling CraftOnPoisonUse restores Wheeler's original action. Inventory F8 still opens the bulk dialog.
- Verify the existing food/alcohol/rune/potion icon appearance and named-potion entries remain unchanged with the companion Wheeler update.

# Inventory and animation checks (not yet run in Skyrim)

- Equip a bow + ordinary arrows, then click a poison: one bottle's dose count is coated immediately, with NO confirmation or batch dialog and no poison on the bow.
- Repeat with a crossbow + ordinary bolts, both mouse buttons, keyboard use and controller use in SkyUI. Measured Dose doubles the one-bottle bolt output; strength perks do not change the amount consumed.
- With abundant poison/ammo, a click still consumes only ONE bottle. With less than one full batch's ammo, consume one bottle and coat only the available count.
- Highlight a poison and press F8: the multi-bottle dialog appears with the correct arrow/bolt counts. Check 1, 5, 10, maximum, Cancel and Escape. Cancel/opening consume nothing.
- Repeated clicks/F8 while a batch dialog is open must not consume another bottle or open another dialog. AutoEquip=1 selects coated ammo; equip ordinary ammo again before a new batch. AutoEquip=0 allows successive deliberate one-bottle clicks.
- Change equipment in an unpaused menu before F8 confirmation: invalid equipment/inputs cancel safely. Selection changes must not change the poison captured by the original request.
- No ammo, wrong ammo type, already-coated ammo, stolen/quest inputs, pre-existing weapon poison: notification only, no normal bow-poisoning confirmation and no consumption.
- Melee poison use still invokes the original weapon-poisoning action. Health potions, food, drop and favorite actions behave normally.
- Close/reopen Inventory and save/reload: routing remains present. CraftOnPoisonUse=0 + restart restores original use; F8 still opens bulk coating.
- Keep Immersive Interactions + supplied New Anims 1.5 and dependencies installed, enable its poison animations and draw the weapon. Click once, then close Inventory: expect one arrow/bolt poisoning animation and only one consumed bottle.
- Repeat F8 with 5 bottles: one animation for the completed transaction, not five. Poisons Used increases by five, not by ammo count. Cancel/failure must not increase the statistic or request an animation.
- Check static poison, native weapon oil, and the last bottle of a player-created poison. Verify displayed poison/ammo models and restored weapon/shield visibility after animation.
- Test first person with the addon's supported camera/offset setup and third person, both ordinary paused Inventory and Skyrim Souls. Check no duplicate animation from natural and targeted event delivery; trace logs report dispatch acceptance, not proof of animation completion.
- Test sheathed weapon, disabled poison animations, AR_IgnoreObject items, another active interaction, rapid crafting, and save/load during a queued animation. Retain addon's skips; no new-session callbacks or persistent hidden weapon.
- ImmersiveInteractionsBridge=0 + restart disables this bridge. Without Immersive Interactions installed, crafting must still work.

# Coating perk runtime checks (not yet run in Skyrim)

Use a copied save and TraceProjectiles=1. Verify actual target health/effect duration as well as logs. Inventory bottle strength does not change: bonuses apply at impact.

- At Marksman 24/25: Coating Mechanist I unavailable/available. At 49/50: II unavailable/available, and II also requires I. At 29/30: Measured Dose unavailable/available independently of I.
- All three nodes visible and selectable with perk points; no overlap with the throwing-weapon branch; existing nodes unchanged.
- Same poison, same bolt and target: no perks 1x, I 1.25x, I+II 1.5x (rank II replaces I). Measured Dose alone leaves strength at 1x.
- Base 5 bolts/bottle: no Measured Dose 5; with Measured Dose 10 at both strength ranks. One bottle is consumed; output count agrees with dialog. Repeat with native dose perks and ArrowsPerBottle override.
- Repeat with a native weapon oil (including elemental and conditional target-specific oil), a static poison, and a player-crafted multi-effect poison. Include a last-bottle custom poison.
- Melee and bows/arrows get no new bonus. Swap away from a crossbow while a bolt is in flight: that bolt still qualifies; an arrow fired before switching to crossbow does not.
- Miss/pick up/re-fire: base stored coating remains unchanged; strength must not compound.
- Save/reload with an active poison: magnitude/duration must not double again. Load another character: no carried-over coating context.
- Huntsman's Satchel: outgoing poison strength is unchanged by the current trait; its drawback is -50 poison resistance. Custom poisoned-ammo proxies are not linked to its ingredient-refund ledger in this build.
- Ordinary crossbow weapon-slot poison should get strength bonus too; Measured Dose only changes this mod's ammunition batch crafting.
- If Coating impact is logged but Coating applied is absent for an effect, capture the log plus ammo/poison name and target; do not claim the strength feature passed.

# Required Skyrim beta checks

These checks have NOT been run in Skyrim by the build system. Run on a copied save in a separate MO2 profile. Record results and attach PoisonedAmmoNative.log. Turn TraceProjectiles on while testing, then off.

| Check | Expected result |
|---|---|
| Inventory hotkey, one bottle, no poison-dose perks | One separate poisoned ammo stack; exact displayed material cost; auto-equipped |
| Cancel / no ammo / quest item / stolen inputs | No materials consumed |
| 7 arrows, 5 doses per bottle, 2 bottles | 7 poisoned arrows; 2 bottles consumed |
| Concentrated Poison or Requiem equivalent | Dialog agrees with native perk dose count; no CTD |
| Modded ammo and poison from two different ESL-flagged ESPs | Correct models, damage and effects; separate stacks |
| Player-made multi-effect poison, consume last bottle | Every effect applies after bottle is gone; save/exit/relaunch retains the batch |
| Player arrow and player crossbow shot | Exactly one native poison application; physical damage unchanged |
| Resistant and immune targets | Normal poison resistance/immunity respected |
| Follower with poisoned arrows, then poisoned bolts | Same poison application as player; no script dependency |
| Miss ground/wall, retrieve arrow, fire again | Retrieved ammo retains the poison |
| Hit actor, recover arrow from body | Ordinary ammo; no retained poison. Recovery perk chance remains normal |
| Save with poison active on a target; reload | Ongoing effect restores without crash or extra application |
| Save with a missed poisoned arrow in the world; reload | Pickup retains poison and can be fired |
| Save with a poison arrow in flight; reload | No crash; poison source and impact behavior remain correct |
| Transfer to follower and unloaded container; save/exit/relaunch | Names and poison recipes restored in both inventories |
| Load a different character without restarting | No recipe leakage between saves |
| Move an ESL source in load order without changing local IDs | Source still resolves. Use a disposable test profile |
| Remove a source plugin in a disposable profile | Warning in log, affected slots remain reserved, no unrelated poison assigned |
| Remove or swap only the .skse co-save in a disposable profile | Mismatch warning; crafting disabled; restoring matching file recovers |
| Wheeler selection, SkyUI list refresh | Newly crafted stack has correct name, count and equipment behavior |

Do not continue saving the main playthrough if any effect, transfer, load, or recovery check fails. Send the log and steps so the native adapter can be corrected.
