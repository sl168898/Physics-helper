# Huntsman's Satchel — native 2.0.4 diagnostic

Replaces Venom Harvester inside Biggie Traits Combined 2.10.4-diagnostic. The DLL
keeps the filename VenomHarvester.dll and the existing VH_Native.Poll binding.
Skyrim Steam 1.6.1170, matching SKSE and AE Address Library are required.

## Current diagnostic status

The 2.0.3 user log confirms the finished poison is now found (one actual
inventory bottle), but the native tracked-copy step is rejected. That warning
combined many distinct checks, so the precise rejection cause is not yet known.
This build preserves every creation/identity check and adds a distinct reason
to each rejection. It logs the original and returned form IDs, poison flags,
marker values, effect counts, and each effect's base ID, magnitude, duration,
area, cost, condition presence, hostility and NoMagnitude status (up to 32
effects per item). Native call entry and successful validation are also logged.

No native-copy fix is claimed by 2.0.4. The next required check is to install it,
arm recording, brew ONE poison, and upload VenomHarvester.log before restarting
Skyrim. That crafting attempt is sufficient to expose the failing check; a
combat test is not required at this stage. The game is not available here.

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
The craft event is only a signal: it may name the DefaultPoison template rather
than the finished poison. The output is the single unbound poison whose count
increased in the inventory during that captured callback. More than one
possible poison output is rejected rather than guessed. Only the added bottle
count is exchanged; pre-existing stock is not included.
It uses BGSCreatedObjectManager to make a distinct engine-created poison for
the batch, with a hidden, zero-cost, empty script effect holding a batch number.
Every real EFIT entry is checked before exchanging the new output, and the
original poison name, weight and alchemy data are retained. Eligible batches
therefore appear as separate inventory stacks even if their displayed names
match. There is no new craftable item recipe or extra ESP.

ValueModifierEffect's native ModifyActorValue operation provides lethal Health
change evidence. There is no broad scan of poisons on TESDeathEvent and no
outgoing magnitude/duration adjustment. Essential actors and nonlethal effects
cannot claim a refund. The lethal native Health change must leave the victim
dying or dead; its confirmed refund is paid on the next game task without
waiting for a second death-state check or requiring the corpse to remain.

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
traits remain. The power and resistance drawback synchronize on game load and
native work tasks (the legacy Papyrus Poll binding is also retained).
Already-active poison effects finish naturally;
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

## Version 2.0.2: confirmed poison kill refund repair

The old payout task skipped victims still in kDying, with no reliable retry,
and lost pending proof if another mod deleted the corpse. The native damage
hook had already proved the poison's killing blow. The payout now uses that
proof immediately and corpse deletion preserves the unpaid candidate. It
still validates ingredient forms and marks the crafting batch paid before
changing inventory. There is no on-death scan or refund for a later weapon kill.
Only Health value changes can qualify at the native hook.

The power displays the number of unclaimed batches for the stored recipe.
The first recipe/batch success message now waits until binding actually succeeds.
Crafting rejection reasons, ingredient expenditures and the first 80 player
poison Health changes per loaded game are logged for diagnosis. If no refund
occurs, send Documents/My Games/Skyrim Special Edition/SKSE/VenomHarvester.log
from that session before restarting Skyrim (the log is overwritten on launch).

The HSAT version-2 co-save is unchanged: old verified batches and unpaid kills
that survived in the co-save can still settle; spent batches stay spent. No
unknown ingredient costs are inferred for older inventory. Regression coverage
includes immediate settlement, deletion before payout, repeated payout tasks,
multiple victims per batch, missing ingredient forms and save/reload.
Windows compilation and native tests do not establish in-game success.

API provenance: pinned CharmedBaryon/CommonLibSSE-NG headers; the created potion
API and Address Library IDs are documented in powerof3/CommonLibSSE's
BGSCreatedObjectManager.h/.cpp. No guessed machine-code instruction offsets.

## Version 2.0.3: DefaultPoison craft-event identity repair

The user's 2.0.2 log confirmed recording was armed, then reported:
`Craft 0005629E rejected: net output 0 bottles`.
0005629E is Skyrim.esm's DefaultPoison, not the unique finished poison. Using
that event form as the output inventory key prevented batch recording.
The event now signals a poison craft while before/after inventory snapshots
identify the actual output and exact new bottle count. Ingredients are still
measured from the same callback. No arbitrary delay or later-inventory scan
can mix purchases, free refills or a subsequent craft into the transaction.

The shared capture helper has regressions for a generic event with zero
template bottles but a positive actual poison output, existing stock, multiple
bottles sharing one refund, ambiguous outputs, absent events, already-bound
poisons, completely/partially free crafts and invalid/overflowing counts.
New logs show the event form and actual output separately. If capture is
rejected, positive alchemy inventory changes are logged for diagnosis.

The native lethal-kill test, corpse-independent payout, menu callback repair,
once-per-batch limit and HSAT version-2 co-save remain unchanged. A new batch
must be brewed to record an expenditure that older versions failed to capture.
The code is compiled and tested; successful gameplay still needs verification.

DefaultPoison identifier reference:
https://github.com/Mutagen-Modding/Mutagen.Bethesda.FormKeys/blob/650e147f854086b47b91ea80a881751466135256/Mutagen.Bethesda.FormKeys.SkyrimSE/Skyrim/Ingestible.cs


## Version 2.0.1: Satchel button crash repair

The power now creates a native MessageBoxData, attaches a reference-counted IMessageBoxCallback to its callback field, and queues it. The old RE::CreateMessage wrapper accepted the wrong callback type: the engine expects a raw function pointer at 51420/52269. Passing a C++ callback object there can execute heap data when a button is pressed. The fixed menu bypasses that helper and its variadic button-list ambiguity entirely.

The callback initializes its inherited state, copies its request into a game-thread task, and never captures its own pointer. Ticket and save-generation checks reject duplicate or stale callbacks. Close/Escape and trait removal do not arm recording. Menu opening failures release the ticket. Recording, stored recipes, refunds, Jarrin Root and the 50% poison weakness retain their existing rules; the HSAT version-2 save format is unchanged. VenomHarvester.log records menu opening, callback and recording state.

Primary technical references:
- CommonLibSSE-NG MessageBoxData and IMessageBoxCallback at b93280e832f263dbef44e44cbe2936622a02f91a.
- MessageBoxData field names: https://github.com/adya/CommonLibSSE/blob/3adc3270274f954caebc165ddcc7a3969596eb1e/include/RE/M/MessageBoxData.h
- Native CreateMessage callback ABI: https://github.com/NoahBoddie/poison-aid/blob/9d4b176553f08de385e08f70d95a492e7cd9a9b8/src/PoisonHandler.h
- Independent description of the same crash and missing variadic terminator: https://github.com/Modding-Forge/xEditLinker/blob/1983a759619fc111e97b0ec3e6f1ad5598e6db05/src/sse/main.cpp

Windows compilation, harvest tests and menu lifecycle tests are required. Skyrim itself has not been run here.
