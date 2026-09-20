# Biggie Trait Mechanics

SKSE helper 1.3.1 for Skyrim 1.6.1170 and Biggie Traits Combined v2.6.1.
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
