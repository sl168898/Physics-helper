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
- Same poison, same bolt and target: no perks 1x, I 1.5x, I+II 2x (never 3x). Measured Dose alone leaves strength at 1x.
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
