# NPC Food Prop Recovery — 0.1.0 test 1

Experimental runtime workaround for vanilla NPCs eating bread or drinking
from a tankard while the prop is missing. Made for Steam Skyrim SE/AE 1.6.1170.

**Status:** compiled and policy-tested, but NOT tested in a running Skyrim
instance. This is a candidate recovery patch, not a confirmed engine-level fix.
An in-game result and the log are needed to establish whether your occurrence
follows the observed-load/draw failure this patch targets.

## Requirements

- Steam Skyrim runtime **1.6.1170.0**.
- SKSE64 **2.2.6** for that runtime.
- Address Library for SKSE Plugins, **Anniversary Edition / 1.6.x** files,
  including the 1.6.1170 database.
- Microsoft Visual C++ 2015–2022 x64 runtime (usual CommonLib/SKSE requirement).

Official SKSE: https://skse.silverlock.org/
Address Library: https://www.nexusmods.com/skyrimspecialedition/mods/32444

No eating mod, ESP/ESL, SPID, Papyrus script or animation generator is required.
Works as a standalone test in an otherwise vanilla game with SKSE and Address
Library. It intentionally refuses to load on other game runtimes.

## Install

1. Install this ZIP as a normal mod in MO2 and enable it.
2. Check that Data/SKSE/Plugins contains NPCFoodPropRecovery.dll and
   NPCFoodPropRecovery.ini in MO2's virtual Data view.
3. Launch via skse64_loader.exe / the SKSE entry in MO2.
4. Enter an inn or another place where NPCs naturally eat. Observe several
   complete eating cycles; the patch only tracks events seen after loading.
5. An identified missing prop must persist for about 1.5 seconds before a
   retry. Asynchronous loading can take additional time.

Keep a separate save for the first test. The plugin does not write save data.

## What the patch actually does

- Observes vanilla NPC animation events; player events are untouched.
- Recognizes AnimObjectBread and AnimObjectTankard.
- Remembers the original AnimObjLoad payload and subsequent AnimObjDraw or
  NPCAnimObjDraw tag/payload.
- Checks the engine's AnimationObjects registry and the attached node.
- If an expected, drawn prop remains absent/detached, asks the actor's existing
  AnimObjLoad and original draw handlers to retry. A hidden attached prop gets
  only the original draw handler.
- Waits for pending asynchronous loads, respects visible=false records, and
  refuses ambiguous duplicate registry entries.
- Stops tracking on hide/unequip/idle exit/movement, actor unload/death, graph
  replacement, save load, or an event-queue overflow.
- Limits each observed sequence to two attempts, with at least 10 seconds
  between them, and expires tracking after two minutes.
- Does not reset AI, respawn actors, change inventory, edit quests, restart
  animations, attach guessed replacement meshes, or modify game behavior files.

Because the exact vanilla failure has not been reproduced here, it can miss
cases where the original load/draw event never occurs, where the engine
explicitly marks the object invisible, or where the registry reports a healthy
prop despite a different rendering problem. Unsupported food props are logged
but not repaired. Custom animation systems have not been compatibility-tested.

## Feedback / troubleshooting

Log:
Documents/My Games/Skyrim Special Edition/SKSE/NPCFoodPropRecovery.log

The log is replaced at each launch. Copy it after a session where the problem
occurs, before restarting Skyrim. Send it with the NPC name/location and whether
the prop was restored.

Useful lines:
- "Ready" confirms initialization.
- "event" shows actual prop/animation requests.
- "state ... missing-or-detached" identifies a targeted failure.
- "Recovery requested" means an attempt, not proof of a visual fix.
- "Recovery observed" means a later check found an attached, unhidden node;
  the user's visual observation is still the final test.

INI options (restart game after editing):
- Enabled=0: do not install the recovery event hook.
- Repair=0: diagnostics only, no load/draw handler retries.
- LogEvents=1: retain event/state details during this test.

For comparison, test Repair=0 first, then Repair=1 under the same conditions.
After checking the result, disable this mod to compare with the original issue.
Do not run multiple versions of this DLL at the same time.

## Remove

Exit Skyrim, disable/remove the mod in MO2, then launch normally. No ESP,
persistent quest, script, inventory item or co-save record needs cleanup.

## Build / verification

The Windows workflow pins CommonLibSSE-NG and vcpkg, compiles with MSVC x64,
runs recovery-policy tests, and packages the DLL with its INI and BuildInfo.json.
The latter records the source commit, dependency commits and DLL SHA-256.

Source:
https://github.com/sl168898/Physics-helper/tree/codex/npc-food-prop-recovery/npc-food-prop-recovery

Implementation references:
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/A/AnimationObjects.h
- https://github.com/CharmedBaryon/CommonLibSSE-NG/blob/b93280e832f263dbef44e44cbe2936622a02f91a/include/RE/I/IHandlerFunctor.h
- https://wiki.beyondskyrim.org/wiki/Arcane_University:Implementation_of_Custom_Animations

Original patch code: MIT. CommonLibSSE license included separately.
No Bethesda game assets are redistributed.
