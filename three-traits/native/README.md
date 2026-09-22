# BiggieTraitMechanics 1.4.0 — Iron Lungs beta

Adds Iron Lungs to the combined package for Skyrim Steam 1.6.1170.
The existing ten traits and Skald/Echoing Steel attack sequence are preserved.

Manual Unrelenting Force costs maximum Stamina times max(0.10, 0.25 times
ShoutRecoveryMult), adds non-elemental magic damage equal to 25% of current
Stamina immediately before payment, and produces no new shout recovery.
The current maximum includes Fortify Stamina. Voice of Authority's +20%
recovery penalty increases the cost; percentage recovery reductions lower it.
All three word levels use the same cost and bonus formula. Their existing
base damage, stagger, knockback, and other winning-override effects remain.

ActorMagicCaster::CheckCast rejects unaffordable manual casts. A second
check wraps VoiceSpellFireHandler::ExecuteHandler before the release occurs.
One payment is reserved per release, and refunded if no UF projectile launches.
Projectile handles retain separate cast snapshots; targets receive one bonus
per cast even if several original effects or projectiles touch them.
MagicCaster::FindTargets supplies projectile context to MagicTarget::AddTarget.
Only accepted original hits qualify. A target-actor helper spell applies the
bonus through the engine's Health-damage magic-effect path, with ResistMagic
and no elemental resistance or armor check. The engine's outgoing and incoming
spell-magnitude perk entry points are evaluated against the original UF spell.
The helper effect has Power Affects Magnitude disabled to avoid applying those
modifiers a second time. No Destruction skill experience is generated.

Skald's instant casts do not pass through the manual voice release handler,
so they incur neither this cost nor bonus. Its existing recovery and Echoing
Steel interaction remain. Normal manual VoiceFire events are retained.
No global shout cooldown multiplier, original shout, or original spell is edited.
No added regeneration penalty. Per-cast state is transient and reset on load;
a projectile already in flight when a save is reloaded has no retained bonus.

## Validation limits

This beta must pass Windows compilation, the four rule suites, and package
record/asset regression checks. These do not substitute for testing inside
Skyrim. The native cast and hit routes, absorption/resistance and interactions
with the user's animation/casting mods still require live verification.
The helper log records up to 200 Iron Lungs events per loaded session.

## Engine references

- CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a:
  ActorMagicCaster, VoiceSpellFireHandler, Projectile::Launch,
  MagicCaster::FindTargets, MagicTarget::AddTargetData, ActorValueOwner.
- powerof3/IndividualNPCShoutCooldowns e0dedfa21adad646eb9c8baa9afea536cf725cc9:
  native VoiceSpellFireHandler callback and voiceRecoveryTime location.
- tiltedphoques/TiltedEvolution fbf72883015dba9e26bd1539d3af9d33fa794116:
  MagicTarget::AddTarget AE address 34526 and FindTargets address 34410.
- Exit-9B/Constellations f136590faf03efca649a98ea3b32dc1080d08e98:
  outgoing/incoming spell-magnitude entry-point argument signature.

## Earlier source documentation

# Biggie Trait Mechanics

SKSE helper 1.3.2 for Skyrim 1.6.1170 and Biggie Traits Combined v2.6.2.

1.3.2 adds a bounded Echoing Steel diagnostic capture. The shipped
SKSE/Plugins/BiggieTraitMechanics.ini enables it; set Enabled=0 and restart
Skyrim to disable both tracing and diagnostic notifications. With no INI,
diagnostics are off. No combat rules, ESP records or saved data formats change.

The capture records trait selection, normal VoiceFire events, Skald arming,
attack phases, attack flags, bonus consumption/expiry, and outgoing melee
HitData immediately before/after the plugin's damage multiplier. It stops
after 200 records per save load. Existing callbacks are used: there is no
new polling script, timer or event listener. Optional notifications say
"primed" on arming and "bonus applied to hit" only when this damage hook
multiplies a positive, finite outgoing power-attack hit. The hit message is
shown once per tracked swing. These verify this plugin's damage change;
they do not measure final enemy HP loss after other mods and engine rules.

IN-GAME DIAGNOSTIC CHECK (not performed in the build environment):
1. Install the full combined archive, with its DLL and INI winning MO2 file
   conflicts. Start Skyrim and load your save. Keep Echoing Steel selected.
2. Manually shout, then land a melee power attack within 5 seconds. Look for
   "primed" and "bonus applied to hit". Repeat with a two-handed weapon.
3. With Skald selected and a stored shout, wait for its recovery, then make
   a power attack to release that shout. Land the FOLLOWING power attack
   within 5 seconds. This second attack should receive Echoing Steel.
4. Exit Skyrim and copy Documents/My Games/Skyrim Special Edition/SKSE/
   BiggieTraitMechanics.log BEFORE restarting. Its first line must say 1.3.2.
   A successful HIT record has echo_applied=true and multiplier=1.50 or 2.00.

The earlier user log contains startup lines only, so it cannot establish
whether Echoing Steel affected damage. The existing and expanded pure rule
tests pass; this release provides runtime evidence rather than asserting an
unobserved gameplay failure was fixed. The real game still needs the check.

Handles rolling block counts, single-attack tokens after blocks/shouts, and
Lab Skeever activation. Burden of Devotion now uses an ESP incoming-spell
magnitude perk (1.5x, MagicBlessing only) and a -100 carry-capacity effect.
All previous blessing vtable hooks and weight/duration rules are removed.
After updating an existing save, remove/reselect Burden of Devotion once
through the trait menu, then obtain a fresh shrine blessing.
MinHook is linked statically; no additional DLL installation is needed.

1.3.1 corrects Skald's menu button mapping. The old code set MessageBoxData
offset 0x4C to 4; this is buttonPressOffset, so the game added 4 to every
selected button index. The result could select another shout, navigate,
clear/cancel, or be discarded instead of storing the clicked shout. The
offset is now explicitly zero, with an offset assertion for the pinned ABI.
The native warningType default is also retained (0x38 is not menu depth).
Menu opening, raw button indices, resolved actions and failed validation now
appear in BiggieTraitMechanics.log. A power attack without a stored shout
logs once per selection/load, so this failure is visible without log spam.
The attack/casting behavior, recovery, save format and all ESP records remain
unchanged. Cast logs say "requested" because the engine API has no result.
After replacing the package, use Store Shout again and check for the
"Skald stored: <name>" notification; a previous choice may have mapped wrongly.

MessageBoxData layout reference:
https://github.com/adya/CommonLibSSE/blob/3adc3270274f954caebc165ddcc7a3969596eb1e/include/RE/M/MessageBoxData.h

Attack history and unused combat tokens reset on loading a save or dying.
No effect is enabled without its corresponding trait ability.
The native carry-weight penalty and power-attack stamina cost are in the ESP.
Original VenomHarvester.dll remains unchanged.

Skald grants "Skald - Store Shout", a lesser power with a paged choice of
learned shouts whose first word is unlocked. Power attacks release the stored
shout's first-word spell; the independent recovery is 10 seconds, 6 at base
Speech 50, and 3 at base Speech 100. It uses gameplay time and pauses in menus.
The chosen FormID and remaining recovery are serialized in the SKSE co-save,
with load-order resolution and asynchronous unlocked-word validation on load.
Changing the choice or reselecting the trait does not reset recovery.
Removing Skald removes its power. The ESP retains its 25% light-melee penalty.
The old cloak and OnHit shout-cooldown-reduction script are retired; the
ability is refreshed automatically once on load or selection for old saves.

An attack consumes any existing Echoing Steel bonus before releasing Skald's
shout. That shout arms the NEXT melee power attack within 5 seconds. Repeated
contacts from the current swing cannot consume this newly armed bonus.
Normal shouted VoiceFire events still arm Echoing Steel. Skald explicitly
arms it once without sending a fabricated VoiceFire event to other mods.
Ordinary shout cooldown is neither spent nor reset by this proc.
Shout selection checks run only when choosing or loading; there is no
Papyrus polling timer. The native player update already used by the other
combat traits advances the recovery and detects attack phases.

First-word spells use the instant magic caster so the power attack animation
continues. Standard aimed shouts fire forward and self shouts affect the
player. A custom target-based spell needs a crosshair actor; concentration
spells cannot be emitted as instantaneous first words. Mods requiring the
normal shout animation, later words or a VoiceFire event may need additional
support. In-game effects, voice audio and combat-animation combinations have
not been tested here. The Windows build and rule tests are package gates.

Lab Skeever activation now uses native TESFurnitureEvent and Crafting Menu
open/close events. Alchemy furniture is identified by its actual workbench type
(5 or 6), not a keyword. The actual AlchemyMenu subtype provides another route.
One completed visit queues one game-thread cast of the existing 20-second
bonus spell. Duplicate exit events do not stack or double-refresh it. An old
visit cannot grant a bonus after loading another save or removing the trait.
The existing bonus perk still supplies 30x potion duration and 1.1x magnitude;
food remains excluded and its 0.5x duration drawback remains unchanged.
There are no notifications. BiggieTraitMechanics.log records detected lab
visits and the result of applying the bonus, for in-game diagnosis.

Technical sources:
- CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a (headers).
- Actor::ProcessHitData ID 37633/38586 and Actor*, HitData& signature:
  https://github.com/KrisV-777/Acheron/blob/ab9d303af7636ad6b25d04d9a61d82e497890249/src/Acheron/Hooks/Hooks.cpp
  https://github.com/D7ry/valhallaCombat/blob/48fb4c3b9bb6bbaa691ce41dbd33f096b74c07e3/src/include/Hooks.h
No external mod source is copied into this helper.
