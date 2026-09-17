# Waterskin Bottle Return — SKSE v1

Target: Skyrim SE/AE executable 1.6.1170, SKSE64 for that runtime, and the AE
Address Library. Uses the user's original Waterskin.esp, Requiem.esp and
Personal Tweaks.esp. Requires the inspected recipe and original item IDs.

Your existing cooking recipe remains unchanged:
1 Waterskin (Full) -> 3 Requiem Bottled Water + 1 original Waterskin (Empty).
There are no new items, ESPs, crafting entries, quests, or Papyrus scripts.
This is an independent WaterskinBottleReturn.dll; it does not replace FSMP or
the working flail DLL.

## Install / replace the previous batch-item patch

1. Exit Skyrim. DISABLE the earlier Waterskin_BottledWater_Return_v1 mod,
   including Waterskin_BottledWater_Return.esp and its script. The native helper
   refuses to operate if that patch still replaces your recipe with a batch.
2. Install this ZIP through MO2. Keep the three original plugins enabled with
   Personal Tweaks.esp winning the WaterskinRecipe record.
3. Launch through SKSE. Use your existing Bottled Water cooking recipe.
   No new ESP needs enabling and no additional load-order entry is created.
4. Use a save from before the batch-item patch for the first test, to avoid
   retaining its unused quest/script or temporary items in the save.

## Behavior

The helper wraps the original cooking/smithing crafting confirmation callback.
It only watches Personal Tweaks.esp|000800, checks that its winning record still
requires exactly one Waterskin.esp|000801 and produces three Requiem.esp|03DFE8,
and verifies the inventory transaction after the original callback returns.
Only then does it add Waterskin.esp|000D61. Cancelling awards nothing. Buying,
looting, or drinking water cannot trigger this callback-based reward. It does
not change the original water/skin stats, effects, scripts, or refill behavior.
It adds no HUD notifications, Papyrus timers, or save serialization.

The hook chains the previous callback implementation. A mod which bypasses or
replaces this confirmation path may require an integration; in-game behavior
with your full modlist must still be tested. It does not guess from inventory
losses outside the known crafting callback.

## First checks

Note your filled-skin / bottled-water / empty-skin counts.
- Cancel crafting once: counts should not change.
- Craft once: expect -1 / +3 / +1.
- Craft three times: expect a further -3 / +9 / +3.
- Buy/loot/drink regular water: no empty skin should be awarded by this helper.
- Save/reload and repeat the original recipe.

Windows compilation and transaction-rule tests are checked during the build.
This DLL has not been tested inside Skyrim in the authoring environment.
If it does not work, send WaterskinBottleReturn.log from your usual SKSE log
folder (normally Documents/My Games/Skyrim Special Edition/SKSE). It records
startup, recipe validation and each watched confirmation's result.

To disable the native helper, exit Skyrim and disable this mod. It has no
persistent quest or script state. Real items it already awarded remain items.
BuildInfo.json contains the source revision, dependency pins and hashes.

## Build from source

Source is under waterskin-native in the Physics-helper repository. Run
`tools/build_windows.ps1` with VS2022 C++ tools, CMake, PowerShell 7 and Git.
For portable tests: `g++ -std=c++20 -I src tests/reward_tests.cpp -o reward_tests`.
No original game or mod assets are in the source repository or build.
