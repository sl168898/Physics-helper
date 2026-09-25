# Huntsman's Satchel — native 2.0.8 beta

Replaces Venom Harvester inside Biggie Traits Combined 2.10.8-beta1. The DLL
keeps the filename VenomHarvester.dll and the existing VH_Native.Poll binding.
Skyrim Steam 1.6.1170, matching SKSE and AE Address Library are required.

## Version 2.0.8: give inventory bottles their native ownership references

The 2.0.7 log successfully creates and binds FF000DB1, then releases temporary
references at 21:22:09. The matching crash at 21:22:15 occurs in Dynamic Tooltips
1.0.5 when its inventory scan calls an item's GetPlayable virtual function.
The object has an invalid virtual-function pointer. The stack does not identify
that object's FormID; the two logs support a lifetime defect but do not prove
which object was dereferenced.

A concrete omission exists in the Satchel exchange: AddObjectToContainer was
called without assigning native created-poison references for the added bottles.
Creation and queued-event owners were then released. The former event test
incorrectly modeled inventory addition as automatically granting a native ref.
The primary poison-aid implementation explicitly calls IncrementCreatedPoisonRef
once per bottle returned to inventory after AddObjectToContainer.

This update reserves one native reference per output bottle before the exchange,
then transfers those references to the inventory when addition returns. Normal
game removal/consumption owns their lifecycle. Failed acquisition or an abandoned
exchange releases only the uncommitted reservation. This is separate from the
creation owner and queued-event owners, and is not a permanent plugin pin.
Save-generation checks prevent rollback against a reused ID after loading.

Read-only diagnostics report the created-object manager's poison/potion entries
and counts before reservation, after transfer, and after temporary cleanup. The
last diagnostic is a separate queued task with no native ownership of its own.
It compares pointers while holding the manager lock and does not dereference a
potentially missing item. It never edits manager maps or reference counts directly.

The corrected regression model separates raw inventory entries from native
ownership. It reproduces an inventory entry outliving its object under 2.0.7,
then checks one and multiple bottles after temporary cleanup, normal consumption,
failed acquisition rollback, and a load reusing the same ID. These are simulations
of native ownership, not a Skyrim runtime test. Existing refund/kill rules and
HSAT v2 / HSAP v1 formats are unchanged. Dynamic Tooltips, CIE and PAPER are not
modified; menu-exit and event-lifetime guards remain in place.

Install with Skyrim closed. For this crash test, load a save from BEFORE the
failed Satchel brew, record and brew one new poison, close alchemy and wait for
the prepared notification, then open inventory. A save already containing a
bad dynamic item is not repaired retroactively by this change. If inventory
opens, apply that fresh poison and let its poison damage kill an enemy. Keep
VenomHarvester.log (and a matching crash log if any) before relaunching. In-game
stability and refunding remain unverified until this check succeeds.

Primary sources:
- https://github.com/NoahBoddie/poison-aid/blob/9d4b176553f08de385e08f70d95a492e7cd9a9b8/src/PoisonHandler.h (RemovePoison2)
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/v1.0.5/src/Modules.cpp (BuildLoreCache)
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/v1.0.5/src/Hooks.cpp (inventory show)
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/B/BGSCreatedObjectManager.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/src/RE/I/InventoryEntryData.cpp

## Version 2.0.7: leave the crafting inventory intact until the menu closes

The new 2.0.6 log records a successful batch at 20:42:07 and release of its
temporary references 25 ms later. At 20:42:14, a later crafting confirmation
crashes in an AlchemyItem destructor through Crafting Inventory Extender's
inventory query. This is a different stack from the earlier PAPER event crash.
CIE 2.6 caches original inventory entries and source indices for the crafting
session. Replacing and removing the source poison inside that session is
consistent with the stale object observed in the new crash. The crash log does
not independently prove which owner first invalidated that object.

Craft callbacks now only capture the actual output and ingredient expenditure,
reserve a batch number, and retain the original native form. They do not call
AddPoison or remove/add inventory. A task after the crafting-menu close event
performs creation and replacement only once UI::GetMenu reports no remaining
CraftingMenu object and the player has left the station. CIE can lazily restart
its cache while GetOccupiedFurniture remains set, so both conditions are
required. This waits through closing animations or a quick reopen; the existing
Poll path retries after closure. CIE resets its session in
its synchronous close-event handler, before that task can exchange inventory.
After furniture release, an ordinary engine GetContainerItemCount query also
lets CIE clear a session restarted during the exit animation. This is needed
because CommonLib GetInventoryCounts iterates inventory directly, and CIE's
RemoveItem hook does not itself check whether the crafting session has ended.

Each captured craft retains its own cost and bottle count. Several crafts with
the same original form become separate marked batches after closure. The
original pre-crafting bottle count is reserved; missing new output causes a
rejection instead of converting that older stock. Source references survive
through menu closure, and both forms remain pinned through PAPER's queued
inventory events as in 2.0.6. There is no timer-based delay or permanent pin.

Pending captures are stored in an additional HSAP v1 co-save record for saves
made before menu closure. Existing HSAT v2 batches and kill/refund rules remain
unchanged. Load/revert clears old-world work without dereferencing old pointers;
restored pending work resolves IDs again. Neither CIE nor PAPER is modified.

For the gameplay check: record a recipe, brew it twice in the same alchemy
session, then CLOSE the crafting menu and wait for the Satchel preparation
notification before applying a new bottle. Expect Staged batch while crafting,
then Crafting menu destroyed and Bound batch after closure, followed by Poison
lethal / Refunded after a poison killing blow. Keep both logs if a crash occurs.
This change has not been tested inside Skyrim; the regression tests model menu
lifetime, repeated crafts, reserved stock, save/load, and shared batch refunds.

Primary source references:
- https://github.com/ohfor/scie/blob/558ddd0c8026cb16bac0fb157cde23d9c0a3edb2/src/Hooks/InventoryHooks.cpp
- https://github.com/ohfor/scie/blob/558ddd0c8026cb16bac0fb157cde23d9c0a3edb2/include/Hooks/CraftingSession.h
- https://github.com/ohfor/scie/blob/558ddd0c8026cb16bac0fb157cde23d9c0a3edb2/src/Hooks/CraftingSession.cpp
- https://github.com/ohfor/scie/blob/558ddd0c8026cb16bac0fb157cde23d9c0a3edb2/include/Hooks/InventoryHooks.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/src/RE/U/UI.cpp

## Version 2.0.6: keep forms alive through queued inventory events

The user's 2.0.5 log confirms native AddPoison succeeded, all real effects and
the marker survived, and the batch was recorded. The following crash was in
PAPER 2.2.4's ItemEventsFilter, reading a null form. Its register held the exact
original poison ID that the Satchel had just removed during the exchange.

PAPER stores container-event form IDs, queues SKSE tasks, and later resolves
them into pointers. Its filter reads each pointer's form ID without a null
check. Removing the last original bottle could destroy that dynamic form
before the queued event used it. The Satchel now acquires native references to
both original and replacement BEFORE the exchange. After removal and addition
have queued their inventory events, it appends cleanup to the same FIFO SKSE
task queue. The references are released after those earlier tasks run, rather
than at the end of the crafting callback. This does not add a timing delay,
keep permanent reference pins, or modify PAPER.dll.

The deferred holder stores form IDs and the save generation. A stale task
cannot dereference an old raw pointer or release a reused ID in a different
save. Failed retention leaves inventory intact and records no batch. The
native AddPoison call and the existing batch/poison-kill/refund rules remain.
The existing HSAT version-2 co-save is unchanged.

A new regression suite models the reported last-bottle deletion and queued
lookup, then tests retention through added/removed events, overlapping crafts,
failed acquisition rollback, release without permanent pins, and a save-load
transition reusing dynamic IDs. It exercises the same retention helper as the
plugin; it does not run PAPER or Skyrim itself.

Primary source references:
- PAPER 2.2.4 event collection and queueing: https://github.com/DennisSoemers/PAPER/blob/8d47b40d8d23ff74ad531abbde5e1cd47938407d/src/OnContainerChangedEventHandler.cpp
- PAPER filter dereference: https://github.com/DennisSoemers/PAPER/blob/8d47b40d8d23ff74ad531abbde5e1cd47938407d/include/OnContainerChangedEventHandler.h
- SKSE task FIFO and Run/Dispose order: https://github.com/ianpatt/skse64/blob/25b72352adb6543fa6d0bd3795780672b2e238e0/skse64/Hooks_Threads.cpp
- CommonLib task callback ownership: https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/src/SKSE/Interfaces.cpp

Install the full combined update and brew a fresh recorded batch. Confirm the
power shows an unclaimed batch, then apply that fresh poison and let its damage
deliver a killing blow. Logs retain native validation and batch details; after
queued event delivery they also report release of temporary item references.
If the game crashes or refunding fails, preserve the new VenomHarvester.log
and any crash log before relaunching. In-game success still requires testing.

## Version 2.0.5: use the native poison creation path

The 2.0.4 diagnostic log confirmed that the original poison was found, and its
tracked copy preserved both real effects and the batch marker, but returned
`poison false`. The copy was rejected before a refundable batch was recorded.
The adapter had called the native `AddPotion` function (SE/AE 35265/36167).
Skyrim has a separate `AddPoison` function (35266/36168), documented by both
CommonLibSSE-GG and CommonLibVR. This update calls that poison-specific path.

The returned object must still be a poison BEFORE copying its original
metadata. No poison flag is forced on a potion, no manager maps are manually
edited, and the marker's magic-effect flags are unchanged. The unique dynamic
form, batch marker and every real effect must still pass the same checks.
The created-object smart-pointer release policy remains in place.

Primary API references (pinned source):
- https://github.com/eddoursul/CommonLibSSE-GG/blob/2053e94fd1c147b36eae2b4338118552fba407e2/src/RE/B/BGSCreatedObjectManager.cpp
- https://github.com/eddoursul/CommonLibSSE-GG/blob/2053e94fd1c147b36eae2b4338118552fba407e2/include/RE/B/BGSCreatedObjectManager.h
- https://github.com/MinLL/CommonLibVR/blob/550cc4fb9114649dcf526d1f3d73d710c5d7003b/src/RE/B/BGSCreatedObjectManager.cpp

Install the full combined update, arm recording, and brew a FRESH poison.
Expect the recorded-batch notification and at least one unclaimed batch in the
power's menu. Apply the fresh bottle and let its poison damage deliver the
killing blow. Keep VenomHarvester.log before restarting if either step fails.
Detailed original/copy and lethal-damage diagnostics remain enabled.
Windows compilation and the existing native tests are required; they cannot
run Skyrim's AddPoison implementation or prove an in-game refund.

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
It uses BGSCreatedObjectManager::AddPoison to make a distinct engine-created poison for
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

API provenance: pinned CharmedBaryon/CommonLibSSE-NG ABI headers and the
CommonLibSSE-GG/CommonLibVR AddPoison references above. Created-object reference
counting is also documented in powerof3/CommonLibSSE. No guessed offsets.

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
