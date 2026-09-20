# Phial Lifetime Trace 0.1.0

Temporary diagnostic for Steam Skyrim SE 1.6.1170, SKSE and Address Library.
This is **not a fix** and does not repair a save or keep a potion alive.

It records custom potion/poison native reference increments and decrements,
reference counts and pending deletion state before/after each call, call
stacks as module names plus offsets, alchemy clear/destruction calls, load
phases and relevant container events. Load and save notifications produce
created-potion table snapshots. It has no update loop or inventory scan.

## Test

1. Install the ZIP as a separate mod in MO2. It adds one new DLL and does
   not replace the White Phial, decanting, Wheeler or tooltip files.
2. Start Skyrim fresh through SKSE. Load a copy of **Save 28**, while the
   two potion definitions still exist. Keep the same mod configuration that
   reproduced the disappearance; do not add a potion-pinning workaround.
3. Repeat what you did between Saves 28 and 29, including the phial refill.
   Stop when the bottles disappear or the game crashes. If you can save,
   use a new slot; do not overwrite Save 28.
4. Before launching Skyrim again, copy `PhialLifetimeTrace.log` from the
   same SKSE logs folder as `skse64.log`. Normally this is under
   `Documents/My Games/Skyrim Special Edition/SKSE/`; redirected Documents
   can change its location. The log is overwritten at the next launch.
5. Return that log, any new crash log, and the new save/co-save if one was
   made. Disable this diagnostic mod when the trace is collected.

A `READY` line confirms all four hooks installed. A `DIAGNOSTIC INACTIVE`
line means initialization failed and no useful reference trace was armed.
No Papyrus logging setting is required. No new ESP or script is installed.

## Interpretation and limitations

`DECREMENT` with references falling to zero or `queued=true`, together with
its stack and the preceding `INCREMENT`/`CONTAINER` events, can identify the
operation that invalidated a still-used form. The complete event sequence
is required: the final release caller is not necessarily the caller that
originally failed to acquire a reference. Destruction during a normal save
load must not be confused with an unexpected deletion during play.

The tracer forwards engine calls unchanged. It does not block destruction,
alter counters, mutate inventory, register a save-data owner or restore
missing definitions. A raw memory overwrite or a deletion path bypassing
these functions may require a different instrumented build. Tracing also
adds timing and disk overhead. Do not use as a permanent gameplay mod.

This environment cannot run Skyrim. A successful Windows compilation and
binary inspection are not an in-game reproduction or a proven root-cause
fix. The accompanying BuildInfo.json records build validation accurately.

## Source grounding

- Alchemy reference methods: [CommonLibSSE bindings](https://github.com/adya/CommonLibSSE/blob/3adc3270274f954caebc165ddcc7a3969596eb1e/src/RE/B/BGSCreatedObjectManager.cpp).
- Runtime structures: CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a.
- Hook relocation and instruction decoding: MinHook, installed from the
  pinned vcpkg baseline. Only the four owned targets are enabled/removed.

Source is included in the package. No saves or private crash logs are
uploaded to the source repository.
