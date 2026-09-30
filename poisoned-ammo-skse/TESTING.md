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
- Test alongside VenomHarvester: its existing penalty remains, Coating Mechanist multiplies the resulting effect once.
- Ordinary crossbow weapon-slot poison should get strength bonus too; Measured Dose only changes F8 batch crafting.
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
