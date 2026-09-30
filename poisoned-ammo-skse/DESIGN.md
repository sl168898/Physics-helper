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
