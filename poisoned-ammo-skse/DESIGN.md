# Native architecture and evidence

The dependency removed here is dynamic item-form allocation/persistence. DPF is itself an SKSE plugin; merely porting Papyrus calls to C++ would not fix form identity. This implementation instead owns 512 ordinary AMMO records and 512 ALCH proxy records in an ESL-flagged ESP. The engine saves references to those stable forms normally. One GLOB stores a 24-bit checksum fingerprint; the recipe definitions live in this DLL's SKSE serialization section.

An assigned slot is never reused in the same save. The engine does not expose an inexpensive complete inventory of all unloaded containers and dropped ammo, so inventory count zero is not evidence a slot is unreferenced. The finite cap is deliberate.

Form references use the originating TESFile filename and the local ID. Light-file status comes from TESFile::IsLight, including ESL-flagged ESPs; FF temporary forms are never mistaken for plugin records. Resolving uses TESDataHandler::LookupForm and checks the light range. Static poisons use their original ALCH; dynamic crafted poisons keep only stable MGEF/keyword references and value snapshots. Conditions on those static MGEFs remain native. Unsupported dynamic conditions/scripts are rejected.

The plugin chains ArrowProjectile vtable entries Handle3DLoaded (0xC0), AddImpact (0xBD), and ProcessImpacts (0xAC). CommonLib's post-1.6.629 accessors locate ammoSource and poison on 1.6.1170. The poison pointer is assigned before native impact processing. Actor contact replaces ammoSource with the original ammo so body recovery is spent; environment hits keep the poisoned record. This hook ordering needs the in-game checks in TESTING.md and must not be represented as verified solely by compilation.

Inventory crafting is a native messagebox reached through a configurable key while InventoryMenu highlights an ALCH. Every callback carries a load-generation token and revalidates equipment, poison snapshot and current quantities. Item removals are checked and partial failures refunded. Output is added only after both inputs were removed. Arbitrary third-party inventory event handlers can still interfere; the checks do not prove compatibility with all such handlers.

The perk call uses ModPoisonDoseCount with three condition tabs: actor owner, equipped weapon and poison, followed by a float output. A fixed INI ratio bypasses this entry point when desired.

Co-save decoding is bounded, versioned, checksummed and atomic. A global-variable fingerprint detects absent/wrong sidecars on post-load. Unresolved sources retain their assigned slot. Revert discards only per-save mappings. Effect allocations are retained until process exit to avoid freeing an Effect still referenced during the engine's load/revert sequence; repeated loading of saves with many crafted recipes can grow this small arena. This is a beta tradeoff, not an unbounded per-frame allocation.

## Primary references consulted (no source copied from the original mod or DPF)

- Original feature description: https://www.nexusmods.com/skyrimspecialedition/mods/123585
- CommonLibSSE-NG pinned API/layout source: https://github.com/CharmedBaryon/CommonLibSSE-NG/tree/b93280e832f263dbef44e44cbe2936622a02f91a
- TES5Edit record definitions: https://github.com/TES5Edit/TES5Edit/blob/dev-4.1.5/Core/wbDefinitionsTES5.pas
- SKSE save/load lifecycle: https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_SaveLoad.cpp
- Native poison-dose entry-point example: https://github.com/kkw1010-dev/HKT/blob/8254180631ff9d16bb68bb8d45d348a499dd14c9/src/Poison.cpp

No claim is made that all reported DPF/ESL bugs have been reproduced. This design avoids needing that system altogether.
