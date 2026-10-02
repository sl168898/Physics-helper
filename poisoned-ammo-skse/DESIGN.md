# Alchemical Potency integration (0.4.0)

`CM_AlchemicalPotency` retains PERK 0x803 in CoatingMechanist.esp. It replaces the
old critical perk without reallocating a record. CTDA purchase gates remain
GetBaseActorValue Marksman >=60 AND HasPerk 0x802 (Measured Dose). The Perk
Adjuster node, other three perks, ESP filename, masters and co-save format are
unchanged. Precision's launch/critical detours and transient shot cache are
removed. MinHook remains needed for the existing inventory-use hook.

Potency.h resolves the winning item records at DataLoaded: Requiem - Alchemy
Redone.esp 807/808/809/80A, Big Tweaks.esp A6D/A6E/A6F/A70. It collects their
Health ValueModifier/DualValueModifier detrimental effects with a magnitude,
including species-conditional damage effects. Missing optional records are
logged and omitted. No plugin masters, effects or keywords are injected. Recipe
proxies retain these original magic-effect pointers, including after reload.

The existing synchronous coating::Scope identifies the impact's source crossbow,
bolt, shooter and native poison. It now remains active at Mechanist strength 1,
so Measured Dose owners do not need either Mechanist rank for Potency. Invalid
or unrelated nested impacts mask the outer scope until they return. The existing
AdjustForPerks wrappers first chain the original native function, then require
this exact source poison and caster, a non-self target, the player as caster,
the Potency perk and a registered oil damage effect. There is no general spell,
scroll, equipped-weapon, physical-hit, or melee multiplier.

Only the ActiveEffect instance magnitude is multiplied by
`1 + max(0, current Alchemy) / 100`. The raw Alchemy actor value is used at impact;
AlchemyModifier and AlchemyPowerModifier are not queried. Nonfinite skill or
magnitude and overflowing results are left unchanged. Effect duration, shared
MGEFs, ALCH records and recipe fingerprints are untouched. Existing native
resistance, condition and damage handling continues normally.

The per-impact duplicate guard and shared AdjustForPerks recursion guard protect
both Potency and Mechanist scaling. After Potency, the existing Mechanist x1.25
or x1.5 magnitude/duration logic still applies. Potency never adds a second
critical roll or edits critical damage. Already running effects retain their
magnitude; new applications read current skill/perks.

Tests include the production Potency.h and Coating.h with engine doubles, plus
the extracted production impact wrapper for its pointer-return ABI. Windows CI
also compiles the real plugin against pinned CommonLibSSE-NG. In-game load-order
behavior still needs confirmation; automated tests do not simulate Skyrim.

# Coating Mechanist balance update (0.2.9)

The rank selector returns 1.0 without a perk, 1.25 for rank I, and 1.5 for
rank II, including when both ranks are owned. The existing impact-scoped
active-effect path is unchanged. The two PERK descriptions match the reduced
bonuses. No physical-damage, magnitude/duration selection, dose-count, form-ID,
or save-format changes are included.

# Factory-created keywords outside TESDataHandler (0.2.8)

The reported rejection identified `LoreBox_quantDTWhoseQuest` with a valid
EditorID but no entry in TESDataHandler's keyword array. Dynamic Tooltips builds
that name from `LoreBox_`, `quantDT`, and the `WhoseQuest` module. Its factory
helper creates a BGSKeyword and assigns formEditorID directly. Neither an array
entry nor a general EditorID-map entry can be assumed for such keywords.

RuntimeKeywords now merges the keyword array with BGSKeyword forms from
TESForm::GetAllForms(). The global map is inspected under BSReadLockGuard; only
keyword forms are included, and no nested engine lookup takes the same lock.
The per-operation index is discarded after capture/restore, not cached across
loads. The same object appearing in both sources is accepted; different objects
with the same case-insensitive EditorID remain ambiguous. Capture still verifies
pointer identity. Missing registries fail before consuming inputs or publishing
a restored proxy. No keyword is recreated, renamed, omitted, or special-cased.

The recipe representation, v1/v2 serialization and fingerprints are unchanged.
The existing array-only providers remain supported. Factory-created provider
keywords must be recreated before the recipe load callback; Dynamic Tooltips
constructs its modules during kDataLoaded. The corrected resolution path also
handles subsequent sessions where the keyword's temporary FormID differs.

Primary source references inspected for this correction:
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/472887e9489025e72cc009cf2aea858604205f0d/src/Settings.cpp
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/472887e9489025e72cc009cf2aea858604205f0d/src/Modules.cpp
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/472887e9489025e72cc009cf2aea858604205f0d/src/Utils.cpp
- https://github.com/QTR-Modding/DynamicTooltipsSE/blob/472887e9489025e72cc009cf2aea858604205f0d/src/plugin.cpp
- https://github.com/eddoursul/CommonLibSSE-GG/blob/2053e94fd1c147b36eae2b4338118552fba407e2/src/RE/B/BGSKeyword.cpp
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/T/TESForm.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/B/BSAtomic.h

# Previous runtime keyword persistence (0.2.7)

The screenshot's keywordSource error comes from dynamic poison snapshots when
keyOf(keyword) has no plugin-backed key. IsDynamicForm is not proof that a
keyword cannot be restored: a provider may recreate a stable named keyword.
PoisonKeywords.h lazily indexes TESDataHandler::GetFormArray<BGSKeyword>() by
case-insensitive EditorID for one capture/restore operation. It does not depend
on the general EditorID lookup map or retain an index across saves.

Capture retains ordinary keyword plugin/local keys. For a dynamic keyword it
requires a nonempty EditorID (maximum 260 bytes), a unique registry entry, and
identity with the actual keyword pointer. It stores the lowercase EditorID.
A repeated registration of the same pointer is allowed; distinct objects with
the same name are ambiguous. Names are resolved again before publishing any
proxy effects/keywords on restoration. restoreAll shares one local index across
its recipes, while crafting creates its own. Missing dependencies leave the
assigned ammunition slot unavailable and reserved, never silently uncoated.

Core::Poison adds namedKeywords. Total static + named keywords remains bounded
to 128. Payload version 1 is emitted byte-for-byte when no recipe has named
keywords, retaining existing ESS fingerprints. Otherwise payload v2 appends a
bounded name list to EVERY recipe after its static keys (empty where unused).
Decode accepts both versions and validates the complete recipe. The SKSE record
envelope remains version 1. The 32 MiB bound covers a full bank at all field
limits. Saves with named keywords require this reader or a newer compatible one.

The original effects, magnitudes, areas, durations, costs and flags are preserved.
There is no damage-type or item-name whitelist. The static poison fast path,
including White Phial Decanting's protected records, remains a direct reference.
Actual custom item scripts, dynamic MGEFs and per-effect conditions still need
separate support. The exact user's keyword EditorID was not supplied; regression
fixtures reproduce the supported runtime-keyword category, not a captured save.

# Crafted poison script detection and diagnostics (0.2.6)

The pinned CommonLibSSE-NG TESForm::HasVMAD implementation obtains a VM handle
and returns whether it differs from EmptyHandle. It does not enumerate attached
script instances. Using it as a reason to reject a dynamic ALCH falsely treats
ordinary Papyrus-addressable crafted poisons as unsupported scripted items.

PoisonSnapshot.h now inspects the handle's actual attachedScripts entry while
holding attachedScriptsLock. It permits an absent/empty entry and native
Potion/Form wrappers, matching Papyrus names case-insensitively. Any other script
class (including subclasses of Potion), or missing type metadata, is rejected.
VM/policy unavailability fails closed. The lock covers script/type inspection;
no VM object pointers escape the scope. Static plugin poisons still use their
original record directly, preserving their own scripts and conditions.

Snapshot capture is shared by initial request validation and the pre-consumption
transaction recheck. It captures every effect with the same stable MGEF key and
all magnitude/area/duration/cost values, plus the original stable keywords and
alchemy flags/value. Dynamic effect records, per-Effect conditions and dynamic
keywords still require explicit persistence support; none are silently dropped.
No actor-value/damage-type filter is used to decide whether a poison is allowed.
The existing v1 serialization and ESS fingerprint algorithm are unchanged.

Each failed snapshot returns a specific reason and, when applicable, a component
FormID/index or attached script name. Request notifications distinguish poison
and ammunition failures. Warnings log details regardless of TraceProjectiles.
The code defect is confirmed from source and regression tests; the user's exact
in-game poison record has not been captured, so this does not claim that every
possible rejection in the user's load order is resolved.

Primary API references (pinned interfaces used by this build):
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/src/RE/T/TESForm.cpp
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/V/VirtualMachine.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/A/AttachedScript.h
- https://github.com/powerof3/PapyrusExtenderSSE/blob/master/src/Papyrus/Util/Script.cpp (independent attachment-check API reference)

# Crossbow animation sequencing (0.2.5)

Capture a reload-event checkpoint immediately before auto-equipping the output.
For a drawn crossbow, hold the original poison in a Papyrus Variable and defer
both Game.IncrementStat and the targeted OnItemRemoved call. This is necessary
because New Anims 1.5's genuine OnItemRemoved callback can itself start the
animation as soon as it sees a larger Poisons Used statistic.

Chain PlayerCharacter Update (vtable 0, slot 0xAD) and its animation event sink
(vtable 2, slot 0x01) on the supported 1.6.1170 runtime. The event hook forwards
the exact event and returns the original result, then records only an atomic
sequence/kind. It observes reload/reloadStart/ReloadFast and reloadStop/
reloadComplete. No input blocking, animation event injection, graph-variable
writes, forced camera switches or raw graph/sink pointers are introduced.

The player's post-update callback does work only while one visual is pending.
ReloadGate consumes actual event evidence and a readable IsReloading graph
state. A true-to-false transition handles missing stop annotations. A stop
annotation cannot bypass a still-true graph state. After completion it requires
0.2 seconds of idle gameplay; a later reload start resets settling. When auto-equip swaps ammunition, positive completion evidence is required;
there is no fixed-time release for an idle-looking first-person graph. With
AutoEquip=0, if no reload occurs, the graph must be readable and idle for one
second before settling.
Menu pause/item/modal checks reset the startup/settle grace periods, including
unpaused inventory. Unknown or stuck state times out after 15 seconds of active
updates by cancelling, never by forcing the poison animation through a reload.
The delta is finite and capped at 0.1 seconds to prevent hitches skipping guards.

Expected ammo may appear after EquipObject returns. Until first matching, only
the original/empty ammo slot is allowed, and presentation remains blocked.
After a match, any weapon/ammo change, sheathing or death cancels. Load-generation
and request-serial guards protect deferred VM work; a newer craft supersedes an
older visual. The VM callback's final equipment/menu/reload recheck runs in an
SKSE game task, not on a Papyrus worker. New Anims still owns the final animation
and its guards; the asynchronous VM boundary and third-party scripts need in-game
verification. A cancelled pre-dispatch wait does not increment Poisons Used.
Crafting, inventory transaction/rollback, projectile hooks and save data remain
independent of all visual outcomes. No third-party scripts/assets are bundled.

Primary API/event references:
- Supplied New Anims 1.5 source/scripts/AR_Ref_AliasScript.psc, OnItemRemoved.
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/A/Actor.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/B/BSAnimationGraphEvent.h
- https://github.com/AugustGal/Manual-Crossbow-Reloading-SKSE/blob/f180385b324690a4ee7d9d0db95cff8191d3b00d/src/CrossbowReloadManager.cpp
- https://github.com/AugustGal/Manual-Crossbow-Reloading-SKSE/blob/f180385b324690a4ee7d9d0db95cff8191d3b00d/src/Hooks.h

# Read-only icon metadata ABI, v0.2.4

PoisonedAmmoNative_GetIconInfoV1(uint32_t ammoFormID) returns zero for unavailable
ammo or inactive sessions. A valid result is 0xA1010000 OR the native type in
bits 0..3, coating type in bits 4..7 and bolt flag in bit 8. Types: 0 none,
1 fire, 2 frost, 3 shock, 4 poison/generic coating. The coating cannot be zero
in a valid result. All other bits are reserved. The provider computes the word
from the original ammo and reconstructed/static poison while restoring the
recipe. The getter copies it under the existing state mutex; it returns no
engine pointers. Reset, missing sources and save faults invalidate metadata.

# Wheeler integration and impact ABI (0.2.3)

Wheeler snapshots poison form ID, optional inventory display name, right-hand weapon ID and ammo ID before closing. After the close animation it calls the optional C export PoisonedAmmoNative_CoatOneV1(uint32_t poison, const char* name, uint32_t weapon, uint32_t ammo). Return 0 restores native behavior only when CraftOnPoisonUse is disabled. Return 1 claims the ranged-poison action, including validation failures, so vanilla bow poisoning never runs as a fallback. Wheeler retains its native melee action and works normally if the ammunition DLL is absent.

The export copies the display name and form IDs and queues an SKSE game task. It does not retain engine pointers or mutate inventory from Wheeler's render/update thread. A pending gate suppresses overlapping queued requests. The load-generation guard rejects stale requests. The game task resolves the inputs again and rejects an equipment change. Explicit poison requests do not require InventoryMenu; the F8 no-argument request still requires it.

Only Wheeler's named one-bottle transaction consumes poison last, after ammunition removal and output verification. This avoids deleting rename metadata on rollback. The named ExtraDataList is resolved afresh immediately before RemoveItem; it is never cached across a frame. Failures before bottle consumption return the ammunition and remove the provisional output. Existing F8 bulk and unnamed transactions retain their previous checks.

See CRASH_FIX.md for the corrected AddImpact ABI. Save serialization, stable IDs, the two ESPs and the perk tree data remain unchanged.

# Inventory use routing (0.2.2)

Chain InventoryMenu::Accept at vtable slot 0x01. A CallbackProcessor proxy wraps only ItemSelect; every other callback is registered unchanged. Each distinct incoming callback receives a stable wrapper slot so reopening the menu does not accumulate wrappers, and already-wrapped functions are not wrapped again. The previous Accept and native item callback remain chained.

SkyUI InventoryMenu.as sends ItemSelect for onItemSelect and AttemptEquip. The wrapper captures the selected ALCH synchronously, verifies it is poison and the player's current right-hand weapon is a bow/crossbow, then invokes requestCraft with the captured poison. That branch commits exactly one bottle without constructing a MessageBoxData. F8 invokes requestCraft without a captured poison, preserving the existing multi-bottle dialog. It does not invoke the normal weapon-poisoning action on Cancel, input validation failure, or a pending dialog. Non-poison items, melee weapons, and disabled routing forward the original arguments to the original callback. No bare prologue overwrite, poisoning-menu address offset, SWF replacement or global weapon-poison event is involved.

The generation guard, poison snapshot, equipment validation, inventory counts, dose perks and transaction checks are shared. The immediate route holds menuPending through the synchronous transaction, releasing it on success, failure or exception. The F8 route holds it until the one-shot callback. No item is consumed while opening the F8 dialog.

ImmersiveAnimation.h is an optional adapter to the supplied New Anims 1.5 script. It locates a running ImmersiveInteractions.esp quest's player reference alias with the exact bound AR_Ref_AliasScript class. After a committed transaction, Game.IncrementStat("Poisons Used", consumed bottles) is dispatched through the VM. Only its completion callback requests that exact script's OnItemRemoved handler, because the genuine inventory event may have run before the statistic update. The script's previousStat and AR_DogUp checks retain its duplicate/busy suppression; these timing assumptions require in-game validation. The actor is never poisoned, no extra inventory event is broadcast, and no OnObjectPoisoned event is generated. The normal RemoveItem event remains genuine.

The completion callback retains the original poison in a Papyrus Variable and posts game work with the current load-generation guard. It re-resolves the alias rather than retaining an alias pointer through load/revert. Animation errors do not reverse a committed batch. The addon retains its camera, menu, drawn-weapon, parkour, busy, model lookup and ignore-keyword behavior. No third-party source or assets are redistributed. Reference: GiraPomba, Immersive Interactions - New Anims 1.5, https://www.nexusmods.com/skyrimspecialedition/mods/117983; supplied source/scripts/AR_Ref_AliasScript.psc, OnItemRemoved.

Primary UI/API references:
- https://github.com/schlangster/skyui/blob/master/src/ItemMenus/InventoryMenu.as
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/F/FxDelegateHandler.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/I/InventoryMenu.h

# Coating perks extension

Three new records in CoatingMechanist.esp (800/801/802), with a separate additive Perk Adjuster JSON. GetBaseActorValue Marksman >=25/50/30; rank II also HasPerk rank I. No base-tree or existing perk edits.

Measured Dose doubles the native/fixed dose count only for bolts before planning the batch. It does not affect the recipe identity or strength. Max accepted per-bottle count expands from 10000 to 20000 to preserve exact doubling at the configuration ceiling. Per-transaction output limit remains unchanged.

Strength is scoped to synchronous ArrowProjectile ProcessImpacts/AddImpact calls, after the ammunition plugin has attached the correct poison. Qualification uses the projectile's actual weaponSource, ammoSource and shooter, not the shooter's currently equipped weapon. Only ActiveEffect::AdjustForPerks calls matching this shooter and exact poison pointer are scaled; preceding native/mod adjustments are chained first. No poisoning of shared forms, added actor spells, temporary actor-value changes or dynamic form creation. No new save records needed. Scope restoration is thread-local/RAII; recursive adjustment and same-scope duplicate scaling are guarded. Effect allocation is engine-owned and no new effect pointers outlive the impact call.

This design assumes the engine creates/adjusts coating ActiveEffects synchronously in an impact path. This must be tested in-game with the trace log; a mod that defers application will not receive the bonus. Script-hardcoded damage is not rewritten by this multiplier.

# Native architecture and evidence

The dependency removed here is dynamic item-form allocation/persistence. DPF is itself an SKSE plugin; merely porting Papyrus calls to C++ would not fix form identity. This implementation instead owns 512 ordinary AMMO records and 512 ALCH proxy records in an ESL-flagged ESP. The engine saves references to those stable forms normally. One GLOB stores a 24-bit checksum fingerprint; the recipe definitions live in this DLL's SKSE serialization section.

An assigned slot is never reused in the same save. The engine does not expose an inexpensive complete inventory of all unloaded containers and dropped ammo, so inventory count zero is not evidence a slot is unreferenced. The finite cap is deliberate.

Form references use the originating TESFile filename and the local ID. Light-file status comes from TESFile::IsLight, including ESL-flagged ESPs; FF temporary forms are never mistaken for plugin records. Resolving uses TESDataHandler::LookupForm and checks the light range. Static poisons use their original ALCH; dynamic crafted poisons keep only stable MGEF/keyword references and value snapshots. Conditions on those static MGEFs remain native. Unsupported dynamic conditions/scripts are rejected.

The plugin chains ArrowProjectile vtable entries Handle3DLoaded (0xC0), AddImpact (0xBD), and ProcessImpacts (0xAC). CommonLib's post-1.6.629 accessors locate ammoSource and poison on 1.6.1170. The poison pointer is assigned before native impact processing. Actor contact replaces ammoSource with the original ammo so body recovery is spent; environment hits keep the poisoned record. This hook ordering needs the in-game checks in TESTING.md and must not be represented as verified solely by compilation.

Inventory use crafts a one-bottle batch immediately; a configurable key opens the native bulk messagebox while InventoryMenu highlights an ALCH. Every callback carries a load-generation token and revalidates equipment, poison snapshot and current quantities. Item removals are checked and partial failures refunded. Output is added only after both inputs were removed. Arbitrary third-party inventory event handlers can still interfere; the checks do not prove compatibility with all such handlers.

The perk call uses ModPoisonDoseCount with three condition tabs: actor owner, equipped weapon and poison, followed by a float output. A fixed INI ratio bypasses this entry point when desired.

Co-save decoding is bounded, versioned, checksummed and atomic. A global-variable fingerprint detects absent/wrong sidecars on post-load. Unresolved sources retain their assigned slot. Revert discards only per-save mappings. Effect allocations are retained until process exit to avoid freeing an Effect still referenced during the engine's load/revert sequence; repeated loading of saves with many crafted recipes can grow this small arena. This is a beta tradeoff, not an unbounded per-frame allocation.

## Primary references consulted (no source copied from the original mod or DPF)

- Original feature description: https://www.nexusmods.com/skyrimspecialedition/mods/123585
- CommonLibSSE-NG pinned API/layout source: https://github.com/CharmedBaryon/CommonLibSSE-NG/tree/b93280e832f263dbef44e44cbe2936622a02f91a
- TES5Edit record definitions: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas
- SKSE save/load lifecycle: https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_SaveLoad.cpp
- Native poison-dose entry-point example: https://github.com/kkw1010-dev/HKT/blob/8254180631ff9d16bb68bb8d45d348a499dd14c9/src/Poison.cpp

No claim is made that all reported DPF/ESL bugs have been reproduced. This design avoids needing that system altogether.
