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
