# Version 2.1.3 beta: corpse-origin area delivery

The latest log confirms poison kills and two nonzero cast requests, but the
user reports camera shake without damage. Earlier `delivered` lines recorded
only the requested magnitude: they did not prove native acceptance or Health
loss. The exact cause of those missing hits is not established from that log.

The supplied Ordinator archive provides a working reference. Its
ORD_Damnation2_Script.OnDying checks MagicDamageFire and calls its blast
Spell.Cast with the dying target as the source and no explicit target. The
Corpse Gas blast record uses Self delivery, two Health-damage effects with
25/15-foot areas and magnitude 150 each, and an explosion attached to each
magic effect. We use that delivery pattern, independently implemented. No
Ordinator code, forms, assets or master dependency are included.

Our four private damage spells now use Self delivery, a 25-foot native search
area, zero-duration Health damage and their matching private explosion. The
corpse's instant caster fires each nonzero damage portion, with the player as
blame actor. There is no separate PlaceObjectAtMe visual or player-origin
TargetActor cast. The corpse's 420-unit radius (about six metres) remains the
actual damage limit; the slightly wider native search area does not increase
it. Our loaded-actor snapshot, space/range/hostility/companion/summon filters,
corpse line-of-sight check, and a second check at application protect excluded
actors. A native MagicTarget.AddTarget wrapper applies matching resistance,
sets the actual per-target effect magnitude, attributes it to the player,
and forwards the effect to Skyrim. Each actor is admitted once per type per
corpse; rejected effects are not retried with direct Health subtraction.
Ordinary spells pass through the wrapper unchanged.

The poison visual selector no longer mistakes spider web strips for a poison
explosion. If no explicit poison explosion is available, it uses the known
vanilla shout shockwave. Private visuals have no physical damage, force,
secondary spell, hazard or projectile; image-space swap and controller
vibration are disabled. Fire/frost/shock retain their loaded elemental visuals.

Diagnostics distinguish:
- `area-cast`: requested amount and eligible enemy count, originating at corpse.
- `area-apply`: native acceptance, input/final magnitude, resistance and Health
  at that call. Acceptance alone is not evidence of damage.
- `health-update`: an observed native Health modification from our effect,
  including actual Health lost, even if the effect updates after the cast.
- `area-result`: attempted and accepted target counts for that type.
- `candidate`: excluded nearby targets, including failed corpse line of sight.

Install the COMPLETE Combined 2.13.3-beta1 package. Both its ESP and
VenomHarvester.dll must win conflicts. The DLL refuses mismatched old area
records and writes a clear error; other Satchel functionality remains active.
The startup log must say 2.1.3. All prior save records and trait IDs are kept.
Use two hostile enemies standing within about six metres of each other; let
your oil/poison kill one and inspect the survivor's Health. Send the fresh
VenomHarvester.log if damage remains absent. A nearby neutral deer is excluded
by the enemy-only rule. Test elemental oil too, and verify the Satchel refund.

The twelve native automated suites and checked eight-record ESP conversion
cover our filters, numbers, compatibility and packaging. They do not prove
Skyrim rendered the burst or applied damage: this is a beta awaiting in-game
confirmation. No change is made to the separate coated-ammo proxy/batch issue.

# Version 2.1.2 beta: corpse explosion target-search crash

The supplied crash-2026-10-02-01-24-39 report shows a different failure from
2.1.1's bleedout fix. Both observers accepted the poison killing blow, and
Satchel logged `Poison lethal`. A queued explosion then crashed during its
nearby-reference search, before the queued ingredient refund could run.

The shipped 2.1.1 DLL return offset +0x43E3D maps to CommonLib's
TES::ForEachReferenceInRange calling TESWorldSpace::GetSkyCell (AE ID 20543).
The DLL loads its world pointer from TES+0x140; the crash reports the invalid
value 0x44FE0000456F4000 passed to that lookup. The pinned TES header guards
its AE member-layout adjustment with SKYRIM_SUPPORT_AE, whereas the build
uses ENABLE_SKYRIM_AE. This target-search path is therefore removed.

The explosion now snapshots handles from all four actor process lists.
Only loaded, enabled actors within the same game space and a 420-unit 3D
radius are collected, with duplicate actors removed. Exterior neighbours
across cell boundaries are included; different world spaces and separate
interiors are excluded. Handles are resolved again and range/space, life,
hostility, teammate, summon and line-of-sight checks run before delivery.
Collection finishes before any visual or damage is emitted.

No global TES/world-space sky-cell lookup is used by the replacement collector.
The exact templated collector used by the native adapter is also compiled in
the regression suite, which covers cell boundaries, all process levels,
missing/unloaded/disabled actors, duplicate entries, 3D distance, separate
interiors/worlds, invalid coordinates and target movement after the snapshot.
The earlier bleedout regression and all Satchel tests remain in the build.

Install Combined 2.13.2-beta1 over the previous Combined package with Skyrim
closed and restart through SKSE. VenomHarvester.log must begin with 2.1.2.
Reload before the crash, store/brew an unclaimed recipe batch, then let its
poison kill the deer. Expect `Poison lethal`, `Refunded ingredient`, and,
with Corpse Explosion selected, `target scan` followed by `burst`. Their
relative order can vary because refunds and explosions use separate tasks.
A nearby hostile enemy within 420 units and line of sight should take the
matching damage type. Repeat indoors and near an exterior cell boundary.

The previous bleedout fix, recipes, once-per-batch refund accounting, save
formats, matching resistance rules, 25% damage and six-metre radius are retained.
No ESP, script or thumbnail change is required. This is a Windows-built beta;
automated tests do not replace an in-game retest of the native engine path.

# Version 2.1.1 beta: poison kills after nonessential bleedout

Fixes the missed killing blows shown in the supplied 2026-10-02 log. The poison
batch was correctly recorded, its marker matched, the player was the caster,
and source checks accepted every Health tick. Each victim entered life state
8 (bleedout) while still above zero Health. The next poison tick crossed zero
and newly queued death, but 2.1.0 required state 0 and substituted zero for the
pre-tick Health. It therefore never offered a refund or a corpse explosion.

The observer now accepts nonessential bleedout as a living pre-damage state
and reads the actual Health before the tick. This applies to both Satchel and
Corpse Explosion. A positive-to-zero Health crossing plus newly queued or
actual death is still required. Entering bleedout alone cannot trigger either
reward. Essential bleedout, pre-existing death queues, corpse ticks, non-Health
damage, nonlethal damage and duplicate batch claims remain excluded.

Regression tests replay all three logged final sequences:
20.117188 -> 3.8125 -> -12.4921875;
20.500595 -> 8.272079 -> -3.9564362;
22.475403 -> 2.0945435 -> -18.286316.
They check one ingredient refund, one correctly capped corpse-explosion budget,
and rejection of essential/previously dying targets. Logs now include the
pre-tick life state and essential status, as well as the real pre-tick Health.

Install the Combined 2.13.1-beta1 package over 2.13.0-beta1 with Skyrim closed,
let its VenomHarvester.dll win, and restart through SKSE. No ESP/script/icon or
save-format changes are needed. The startup log must show native version 2.1.1.
Reload a save before the failed kill, or use a fresh unclaimed batch on a fresh
target. The old log contains no queued refund, so old kills cannot be paid
retroactively. Expect Poison lethal followed by Refunded ingredient; with
Corpse Explosion selected, expect a confirmed killing blow and one later burst.

This fix addresses the confirmed dagger-test failure. Coated-ammunition proxy
identity matching is a separate compatibility check and is not changed here.
Windows tests are automated; an in-game retest in the user's load order remains
necessary.

# Corpse Explosion / Huntsman’s Satchel 2.1.0 beta

Combined package requirement: 2.13.0-beta1, Skyrim Steam 1.6.1170.

Corpse Explosion is a selectable trait with a 50-point maximum Health penalty.
A confirmed player poison/weapon-oil killing blow produces one burst after death.
The burst deals 25% of actual Health damage personally dealt to that victim;
weapon, enchantment and spell damage contribute, follower damage and overkill do not.
Each nearby hostile enemy within 420 units (about six metres) is eligible, with
line of sight to the corpse. The player, teammates and player-controlled summons
are excluded. The corpse remains intact and lootable.

The lethal oil controls the damage mix, proportional to its recorded fire, frost,
shock and poison Health damage. Each portion checks only its matching resistance;
100% or greater resistance blocks that portion, and negative resistance amplifies it.
The private native spells bypass generic magic resistance/absorption after this
calculation and disable magnitude perk scaling, avoiding double resistance or
Coating Mechanist/Alchemy/Destruction amplification. Native damage delivery remains
responsible for difficulty scaling, kill credit and essential actors.

Weakness-only oils cannot deal a killing blow. Having an oil active when a weapon
or follower kills is insufficient. Explosion damage does not contribute to future
explosions and explosion kills cannot trigger chains. Static poisons, crafted
poisons and ammunition-delivered oils are observed independently of Satchel recipes.

The observer snapshots source identity before the engine can destroy ActiveEffect.
Nested native callbacks share exclusive Health-loss accounting. Settlement waits
until the outermost callback, and the visible burst waits for the actual corpse.
Actor handles/IDs, never borrowed ActiveEffect pointers, survive deferred tasks.
CEXP v1 co-save data preserves ongoing damage, pending bursts and consumed corpses;
older saves without CEXP start tracking on load. Save-generation changes invalidate
queued work. Fully healed actors out of combat start a fresh damage tally.

The two existing Satchel co-save records and all crafting/refund behavior remain.
Tests cover nested callbacks, mixed oils, player attribution, overkill, immunity,
negative resistance, queued death, duplicate suppression and serialization.
An actual Windows build is required; no in-game runtime test is claimed.

## Previous Satchel documentation

# Huntsman's Satchel 2.0.13 beta

Requires Combined 2.12.0-beta1. Base refund is one original ingredient set per batch, with current harvest perk bonuses evaluated for each ingredient at payout. Green Thumb is applied once through the shared native harvest entry point. The Alchemy 50 bonus is removed. Existing batch IDs, original costs, paid flags, rename support, and save formats are unchanged.

# Huntsman's Satchel Rename Potions compatibility 2.0.12 beta

Fixes custom names disappearing when Satchel replaces a brewed poison with its
tracked copy after leaving the alchemy station. The captured inventory name is
preserved in pending save data and copied into the new inventory stack. Source
bottles are selected by their own name, including when one base poison has several
custom names. Earlier named bottles are reserved; uncertain transfers are skipped.

The 2.0.11 Alchemy 50 double ingredient return, once-per-batch refund ledger,
poison damage detection and inventory lifetime fixes are retained. HSAT v2 is
unchanged; pending HSAP v2 records include names and the loader also reads v1.

Install the updated combined traits package and keep the Wheeler/I4 v1.3.3 patch.
The file this update changes is SKSE/Plugins/VenomHarvester.dll. It does not replace
RenamePotionsSKSE.dll, wheeler.dll or InventoryInjector.dll. Start Skyrim afresh.

For a new batch, choose its name at the alchemy table before brewing, then leave
the table normally. The name should remain on the Satchel-tracked bottles.
Names already discarded by an older version cannot be reconstructed reliably;
this update does not invent those lost names. Existing tracked batches keep their
refund records. Validation is automated; no in-game test was available here.

The supported 1.6.1170 extra-data list layout is 32 bytes. This build allocates
through Skyrim's heap and initializes each list with the native constructor
(Address Library IDs 11437/11583). The pinned CommonLib AE facade does not supply
usable construction/destruction or allocation size. Unattached lists clean up
only their owned data chain and bitfield; lists transferred to inventory are
released to engine ownership. The native constructor precedent is Wheeler's
UniqueIDHandler and poison-aid's Utility.h:
https://github.com/NoahBoddie/poison-aid/blob/9d4b176553f08de385e08f70d95a492e7cd9a9b8/src/Utility.h

Earlier documentation follows for reference:

# Huntsman's Satchel — native 2.0.11 beta

Replaces Venom Harvester inside Biggie Traits Combined 2.10.11-beta1. The DLL
keeps the filename VenomHarvester.dll and the existing VH_Native.Poll binding.
Skyrim Steam 1.6.1170, matching SKSE and AE Address Library are required.

## Version 2.0.11: Alchemy 50 ingredient refund

A qualifying killing blow returns one set of recorded ingredients below learned
Alchemy level 50, or two sets at level 50 and above. The base skill is read at
payout, so temporary buffs are excluded and already-recorded unclaimed batches
benefit immediately after reaching 50. No harvesting perk or Botanist is needed.
Every ingredient's recorded quantity is multiplied by the same set count.

The batch is still claimed once before inventory callbacks run. Recorded costs
and HSAT v2 / HSAP v1 save formats are unchanged. Already-refunded batches cannot
pay again. The poison must still deliver the killing blow. Jarrin Root remains
a one-time gift and the 50% poison weakness is unchanged.

The Satchel menu shows the current number of ingredient sets. Refund logs record
the base Alchemy value and set count. Combined 2.10.11 also updates the existing
trait/power descriptions and preserves the Arcane Dynamo 1.6.1 charge correction.
The earlier aborted harvesting-perk experiment is not included.

Tests cover the 49/50 boundary, unequal ingredient counts, all three ingredients,
old saved batches, missing ingredient validation, and duplicate claims through
multiple doses or save/reload. Skyrim runtime verification is separate.

## Version 2.0.9: observe the native poison damage correctly

The user's 2.0.8 test no longer crashed. Its log confirms a recorded batch,
three consumed ingredients, a valid native poison, and exactly one native
reference after temporary cleanup. It contains no poison-modification, lethal,
or refund entries. Creation and inventory ownership succeeded; the detector
was silent before any refund was offered.

Two verified adapter defects blocked valid calls:
1. ModifyActorValue can receive ActorValue::kNone, meaning use the effect's
   actorValue member. The old hook tested only an explicit Health argument.
   It therefore ignored native implicit-Health calls, including its diagnostics.
   The observer now resolves this sentinel; the original native call still
   receives the original argument, so actual damage is not changed.
2. The pinned CommonLib GetTargetActor implementation reinterpret_casts the
   secondary MagicTarget pointer into an Actor pointer. On Skyrim 1.6.1170,
   AsMagicTarget returns Actor+0xA0. Comparing that reinterpreted address against
   the native target Actor pointer rejected the same actor. We now compare
   effect->target directly to nativeTarget->AsMagicTarget(). No guessed cast or
   hard-coded runtime offset is used in production.

Native poison code also demonstrates that a lethal operation can set killQueued
before the victim's life state becomes dying/dead. The observer accepts a newly
queued death only when that same native Health operation crosses from positive
to zero/negative Health and the victim is not essential. Already-queued deaths,
corpse ticks, non-Health changes, nonlethal poison, and bleedout do not qualify.
Immediate dying/dead transitions continue to qualify. This is still native
poison killing-blow evidence, not an on-death scan of active poisons.

Diagnostics now capture raw/stored/resolved actor values, source and batch,
caster, target, Health before/after, life state, killQueued transition, source
rejection reason and nested lethal attribution. The first 160 poison operations
per loaded game are logged, including non-Health and non-player rejections.
All effect-derived data are captured BEFORE the original call, because it may
destroy the ActiveEffect. The original function is called exactly once and its
arguments are unmodified. Actor lifetime is retained across the call as before.

The new regression suite reproduces the kNone rejection and the secondary-target
address mismatch, checks explicit AV precedence and immediate/queued deaths,
rejects nonlethal/essential/prequeued/corpse cases, and connects the corrected
observation to one shared batch refund. Tests model these conditions; they do
not execute Skyrim. The Windows build and gameplay check remain separate gates.

Install with Skyrim closed. Load a save before the consumed test bottle if it
is available, or record and brew a fresh batch. Close alchemy and wait for the
preparation notification. Apply the tracked bottle, hit a living nonessential
enemy, and let the poison deliver the final damage. This update does not infer
refunds for kills the old detector never recorded. Keep VenomHarvester.log
before relaunching if a refund still fails; it should now show Poison modification
with an explicit reason, followed by Poison lethal and Refunded when eligible.

The one-reference-per-bottle repair, menu-exit guards, event retention, crafting
ledger, one-refund allowance, gift guard, and HSAT v2 / HSAP v1 formats remain.
No ESP, other trait, thumbnail, script, configuration, or third-party DLL changes.

Primary sources:
- https://github.com/alandtse/FloatingDamageNG/blob/01d57836beb6e4bade622dee9d0ec1a6074f1336/src/Capture.cpp (OnEffectModify resolves kNone)
- https://github.com/NoahBoddie/poison-aid/blob/9d4b176553f08de385e08f70d95a492e7cd9a9b8/src/Hooks.h (PoisonBlameHook handles kNone and killQueued)
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/src/RE/A/ActiveEffect.cpp (GetTargetActor cast)
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/A/Actor.h (runtime AsMagicTarget accessor)
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/V/ValueModifierEffect.h (slot 0x20 ABI and actorValue member)
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/M/MiddleHighProcessData.h (killQueued)

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
