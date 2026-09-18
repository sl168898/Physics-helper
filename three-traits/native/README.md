# Biggie Trait Mechanics

SKSE helper 1.1.0 for Skyrim 1.6.1170 and Biggie Traits Combined v2.4.1.
Handles rolling block counts, single-attack tokens after blocks/shouts, and
player-only shrine blessing modifiers checked when the blessing is received.
MinHook is linked statically; no additional DLL installation is needed.

Attack history and unused combat tokens reset on loading a save or dying.
No effect is enabled without its corresponding trait ability.
The native carry-weight penalty and power-attack stamina cost are in the ESP.
Original VenomHarvester.dll and all previous trait gameplay remain unchanged.

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
- InventoryChanges::GetInventoryWeight is carried load; Actor::GetTotalCarryWeight
  is capacity. This distinction is corroborated by OAR's inventory-weight condition:
  https://github.com/ersh1/OpenAnimationReplacer/blob/f4e7688b065175aff70aa523073857911e15aca3/src/Conditions.cpp
- Actor::ProcessHitData ID 37633/38586 and Actor*, HitData& signature:
  https://github.com/KrisV-777/Acheron/blob/ab9d303af7636ad6b25d04d9a61d82e497890249/src/Acheron/Hooks/Hooks.cpp
  https://github.com/D7ry/valhallaCombat/blob/48fb4c3b9bb6bbaa691ce41dbd33f096b74c07e3/src/include/Hooks.h
No external mod source is copied into this helper.
