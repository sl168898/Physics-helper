# Direct flail SMP prototype

Experimental native extension built inside FSMP 3.2.1. The goal is a separate
physics system for each visible flail model, including dual-wielded instances,
without an armor-slot driver or Papyrus global-reset timer.

**Source prototype. A successful DLL build is not an in-game validation.**

## Public repository scope

This repository contains generated C++ source, a patch against GPL FSMP,
portable tests, and a Windows build recipe. It contains no uploaded mod ESP,
meshes, textures, physics XML, or encoded copies of those files.

The workflow builds a **native-only** artifact. A separately prepared companion
ESP and the original mod assets are required for a complete runtime test.
Those mod-derived files are assembled locally and are not uploaded here.

## Implementation

- Resolves each Flail_0 through Flail_4 chain within its own visible subtree.
- Rebinds when visibility, attachment ancestry, or bone pointers change.
- Waits for asynchronous simulation before detaching scene references.
- Suspends direct binding if the legacy armor driver is detected.
- Runs direct systems even without an active armor/hair physics skeleton.
- Handles new games, save loading, and shutdown.
- Uses file logging without HUD notifications or console reset commands.

Scope is player third-person silver flails. First-person, NPCs, mixed flail
types, saved-script migration, visible skinning, and performance need further
runtime work or validation. The portable scanner tests do not exercise Skyrim.

## Build

Run tools/build_windows.ps1 on Windows with Visual Studio 2022 C++ tools, a
Windows SDK, Git and CMake, or use the included GitHub Actions workflow.
Branches named codex/flail-* trigger it automatically.

Dependencies are pinned to FSMP v3.2.1 commit
1bdafeff57cc7a93fd940ef899490ce60053b750 and its submodules, with vcpkg baseline
60b06921c7c7ac787b23a222dfab5cdd3911712e. The build writes only under _build.
It does not install into Skyrim. See RUNTIME_TESTING.md before any game test.

FSMP source: https://github.com/DaymareOn/hdtSMP64/tree/v3.2.1
License: GPL-3.0-or-later, with upstream EXCEPTIONS retained.
