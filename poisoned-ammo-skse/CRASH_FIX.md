# Poisoned bolt impact correction, 0.2.3

The supplied 2026-09-30 log reports Skyrim 1.6.1170 crashing at
SkyrimSE.exe+07EC0B6 / relocation 44204+0x3A6, while processing an Iron Bolt
impact. It writes a byte at [r13+0x48] with r13=0x408. Projectile::ImpactData
has an impact flag at offset 0x48. The log does not contain enough information
to reconstruct every instruction before the fault.

The 0.2.2 plugin chains ArrowProjectile virtual slot 0xBD (AddImpact) with a
void-returning thunk, following the pinned CommonLibSSE-NG declaration. The
native function actually returns Projectile::ImpactData*. An independent
reverse-engineered binding declares this pointer return for Projectile,
MissileProjectile and ArrowProjectile, with a uint32 shape key and bool final
parameter:

- https://github.com/KernalsEgg/SKSE64Plugins/blob/9060187900d3df632d010127ac612fafc6775e5f/Shared/Shared/Include/Shared/Skyrim/P/Projectile.h
- https://github.com/KernalsEgg/SKSE64Plugins/blob/9060187900d3df632d010127ac612fafc6775e5f/Shared/Shared/Include/Shared/Skyrim/A/ArrowProjectile.h

The void wrapper does not preserve the returned pointer across coating-context
cleanup, including the shooter's reference release. This is a concrete ABI
defect and is consistent with the invalid pointer in the supplied log. Version
0.2.3 explicitly uses the native pointer-returning signature and returns the
chained result. C++ preserves that result while running scope destructors.
Arguments and null returns are forwarded unchanged. No impact object is
invented, no collision is skipped, and no exception handler hides the fault.

The regression test extracts the actual production ImpactHook from main.cpp,
compiles it with minimal engine test doubles, and checks its exact signature,
argument forwarding, actor/world contact detection, pointer and null returns
after a cleanup call returning 0x408. It is run in the Windows Release build
and can also run as a portable C++ test. This tests the wrapper contract; it
does not execute Skyrim's collision engine.

CoreImpactFramework, SanguineSymphony and SplashesOfSkyrim appear in the crash
stack. That fact alone does not implicate them. This fix changes our plugin's
ABI; it does not require removing those mods. A repeat shot with the user's
full load order is still needed to confirm that the reported crash is resolved.

No ESP, perk placement, recipe form ID, recipe capacity or co-save format
changes are made. Existing poisoned ammunition receives the new hook after
restarting the game with the updated DLL.
