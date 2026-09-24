# Huntsman's Satchel — native 2.0.0 beta

Replaces Venom Harvester inside Biggie Traits Combined 2.8.0-beta1. The DLL
keeps the filename VenomHarvester.dll and the existing VH_Native.Poll binding.
Skyrim Steam 1.6.1170, matching SKSE and AE Address Library are required.

Select the trait, cast the Huntsman's Satchel lesser power, choose **Remember
next poison**, then brew a poison. Its exact ingredients become the stored
recipe. Future batches brewed with those ingredients qualify too. Only one
recipe is stored; use the power again to replace it.

A kill caused by the stored poison's own native Health damage returns the
ingredients actually consumed by its crafting action, once. All output bottles
and all weapon hits from a batch share that one allowance. The actual lethal
poison determines the refund when several poisons affect an enemy. A weapon,
shout or other attack finishing a poisoned target does not qualify. Paralysis
and weakness alone do not kill. Purchased bottles, White Phial refills, old
untracked bottles and completely free crafts have no refundable expenditure.
Reselecting a recipe or trait never resets spent batches. Rebrewing reclaimed
ingredients is a new crafting action and can earn a new refund.

The trait grants one Jarrin Root per character on first selection, including
the first load of an existing Venom Harvester character after this update.
A saved FormList marker and co-save state guard against repeated gifts.

**Drawback: 50 percentage points less poison resistance.** Outgoing poison
magnitude and duration retain their normal values. This replaces the old 25%
weaker-poison penalty. Resistance gear and other modifiers still combine with
the weakness normally; this is not an unconditional final-damage multiplier.

## Implementation and compatibility limits

The adapter wraps documented AlchemyMenu callbacks and its craft confirmation,
captures ItemCrafted, and measures actual ingredient consumption and output.
It uses BGSCreatedObjectManager to make a distinct engine-created poison for
the batch, with a hidden, zero-cost, empty script effect holding a batch number.
Every real EFIT entry is checked before exchanging the new output, and the
original poison name, weight and alchemy data are retained. Eligible batches
therefore appear as separate inventory stacks even if their displayed names
match. There is no new craftable item recipe or extra ESP.

ValueModifierEffect's native ModifyActorValue operation provides lethal Health
change evidence. There is no broad scan of poisons on TESDeathEvent and no
outgoing magnitude/duration adjustment. Essential actors and nonlethal effects
cannot claim a refund. A completed death is required before inventory changes.

The standard alchemy menu and mods using that menu are supported by this
capture path. Script-only or custom-menu crafting and scripted kill effects
outside native Health modifiers need a separate adapter. Delayed extra bottles
created after the crafting callback are not bound. Conditional EFIT entries
or failed native identity checks leave the original inventory intact and log
the rejected batch. In-game behavior and interactions with the installed
portable-alchemy mod have not yet been verified.

## Upgrade and validation

Exit Skyrim and replace the previous combined package. Keep the same ESP;
replace VenomHarvester.dll and keep BiggieTraitMechanics.dll from the combined
release. The original trait FormID, FLM identity, existing scripts and other
traits remain. The new power and resistance drawback synchronize within the
existing half-second poll. Already-active poison effects finish naturally;
new applications use normal outgoing strength. Brew a new batch after choosing
the recipe: earlier poison inventories have no known ingredient provenance.

This is an implementation beta. The build runs the native rule tests. They
cover actual lethal-source selection, shared multi-hit/output budgets, recipe
switching, rebrewing, consumed counts, save remapping and malformed saves.
These tests do not run Skyrim or substitute for gameplay verification.

For an in-game check, compare poison resistance before/after selection, record
a recipe with the power, brew two batches, and poison two targets with one
batch. Only the first poison kill should return its ingredients. A weapon kill
should return none. Save/reload and switch recipes to check that used batches
stay used. Test with multi-hit poison perks and the portable alchemy equipment.
VenomHarvester.log contains Bound batch, Poison lethal and Refunded messages.

API provenance: pinned CharmedBaryon/CommonLibSSE-NG headers; the created potion
API and Address Library IDs are documented in powerof3/CommonLibSSE's
BGSCreatedObjectManager.h/.cpp. No guessed machine-code instruction offsets.
