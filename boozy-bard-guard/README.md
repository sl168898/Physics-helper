Boozy Bard Guard 1.3.0 beta

For Skyrim Steam 1.6.1170, SKSE and Address Library. Used with the LoreRim Boozy Bard ESP.

Intercepts the player's native MagicTarget::AddTarget call for the patch's two core alcohol effects only. Additional applications are refused while an existing effect or a pending application occupies the slot. Other effects use the previous hook unchanged. No Recast is not used as the enforcement mechanism.

After loading a save, and after protected effect application events, duplicate protected effects are dispelled using Skyrim's native ActiveEffect::Dispel, retaining the oldest active effect. No manual changes to player base health or armor. Pending reservations prevent same-frame applications slipping through before the first effect appears in the list. There is no periodic scan or update registration.

The first active armor/health bonus stays until expiry. Another drink cannot refresh or replace it. Secondary drink bonuses retain the prior ESP's behavior. Ordinary spells, potions, food, NPCs and original Requiem effects are not targeted. Existing duplicates using the patch's FormIDs are cleaned on load; effects from unrelated plugins are not removed.

Logs: Documents/My Games/Skyrim Special Edition/SKSE/BoozyBardGuard.log. The log identifies resolved effects, hook installation, refused applications and save cleanup. Missing dependency or hook errors are logged; a hook-install failure also displays one notification.

The DLL chains the existing player MagicTarget vtable entry, validates the live player vtable against CommonLib's known tables and refuses unknown tables. A later plugin replacing that hook is reported on save load. Runtime support is intentionally restricted to 1.6.1170.

Validation: build-time policy tests for mixed drinks, identical drinks, save stacks, pending applications, expiry and unrelated effects. Engine integration requires in-game testing; successful compilation is not gameplay verification.
