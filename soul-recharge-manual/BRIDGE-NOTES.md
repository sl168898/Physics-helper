# Original 1.1.0 bridge verification

Only the uploaded `AGH_SoulRecharge_G0.dll`, SHA-256
`36716c8750e1acad6d48bda14e127c9ebc2a24fcf002049f39f6d8551339cbc8`, is supported.
The DLL on disk is never edited. The native companion verifies its disk hash and
the relevant loaded instructions/pointer before installing two in-memory hooks.

All offsets below are RVAs in the original DLL, not Skyrim.exe addresses.

| RVA | Observed behavior / use |
| --- | --- |
| `0x51060` | Six-stage recharge kernel. Initializes stage and consumption markers, invokes each stage, and clears its active flag before returning. |
| `0x33700` | Six bytes `48 8B 01 FF 60 08`: invoke context virtual method 1 (stage zero / preflight). A six-byte indirect jump redirects this thunk to the companion. Its ordinary path performs exactly the displaced virtual dispatch. |
| Receipt `+0x288` | Manual-request byte, set to 1 at `0x2B630` by the original F11 dispatcher before the call to the kernel. |
| Receipt `+0x298` | Original `std::string` stop reason. The original string assignment routine at `0x21830` writes `MANUAL_MODE` on rejected automatic requests. |
| Receipt `+0x284` | Consumption-started marker. The kernel sets it only on entering stage two. Rejection at stage zero therefore leaves the error latch clear. |
| `0x2B66F`, `0x52C9B`, `0x54027` | The three direct call sites to the shared kernel: the explicit manual dispatcher and the automatic paths. |
| `0x949E8` | Original input sink's ProcessEvent vtable slot; initially points to `0x2AC00`. The companion supplies only the selected recharge key or diagnostic key to this original handler. |
| `0xB744C`, `0xB7450` | Original INI recharge and diagnostic keyboard scancodes read by that handler. |

The switch is independent of the original `Enabled` setting. The latter is a
master switch, also checked by the original manual command. Temporarily enabling
it to execute manual requests would introduce unwanted auto-recharge windows;
this bridge does not do that.

The menu's mode flag is atomic. A request observes the flag when entering
preflight. If a recharge has already entered its consumption stages, it completes
its original verification rather than being interrupted halfway through an
inventory change. Requests still waiting to reach preflight see the new mode.

The original manual handler queues its own work and performs its existing busy,
world-state, generation, inventory, and charge checks. The companion never adds
souls, consumes gems itself, overwrites enchantments, changes global recharge
settings temporarily, or synthesizes input to other mods.

Settings use the supplied `AGH_SR_Native.pex` ABI: `GetSnapshotJson()` returns a
string and `ApplySettingsJson(string)` returns a bool. Calls are dispatched through
Skyrim's VM, then the settings are read back. No private settings-object layout is
written. Legacy `.user.json` storage remains owned by the original mod.

`tests/core_tests.cpp` loads the original DLL in an ordinary Windows process and
executes the real kernel with fake stage callbacks. This provides a direct check
of the bridge's offsets, stage ordering, dry-run behavior, allocator boundary,
manual/automatic routing, mode switching, and error-latch behavior without invoking
any Skyrim inventory methods. It does not establish in-game compatibility.
