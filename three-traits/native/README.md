# Biggie Trait Mechanics

SKSE helper for Skyrim 1.6.1170 and Biggie Traits Combined v2.4.
Handles rolling block counts, single-attack tokens after blocks/shouts, and
player-only shrine blessing modifiers checked when the blessing is received.
MinHook is linked statically; no additional DLL installation is needed.

Attack history and unused combat tokens reset on loading a save or dying.
No effect is enabled without its corresponding trait ability.
The native carry-weight penalty and power-attack stamina cost are in the ESP.
Original VenomHarvester.dll and all previous trait gameplay remain unchanged.

Technical sources:
- CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a (headers).
- Actor::ProcessHitData ID 37633/38586 and Actor*, HitData& signature:
  https://github.com/KrisV-777/Acheron/blob/ab9d303af7636ad6b25d04d9a61d82e497890249/src/Acheron/Hooks/Hooks.cpp
  https://github.com/D7ry/valhallaCombat/blob/48fb4c3b9bb6bbaa691ce41dbd33f096b74c07e3/src/include/Hooks.h
No external mod source is copied into this helper.
