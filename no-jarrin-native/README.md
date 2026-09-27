# No Jarrin Planting 5.0.0 beta

Native SKSE component for the More Plants base mod update. Targets Steam Skyrim
Special Edition 1.6.1170, its matching SKSE64, and the AE Address Library.
Requires the combined v5 package's `more plants all.esp` marker quest, local ID
0xD66. This native-only CI artifact is not the complete user installation.

The plugin resolves HearthFires.esm's plantable seed list (0x8247), crop list
(0x8246), and Skyrim.esm's Jarrin Root (0x1BCBC). On the game thread it replaces
every root seed slot with the persistent non-inventory marker quest. This blocks
HasForm/Find membership without changing list length, other seed indices, the
crop list, list allocation, or script-added counts. It works on plugin-backed
entries as well as runtime-added entries. No ingredient or harvest records change.

Checks run after a save/new game, FLM_SetupDone, player activation, and opening
the container menu. No repeating update, executable patch, or Papyrus removal
call is used. Queued checks from a previous save generation are discarded.
Only the standard Hearthfire seed list is targeted. A custom planter using a
different list or hard-coded item handling is outside this component's scope.

The old alias PEX is replaced by an inert compatibility script in the combined
package. Existing quest/alias identities are retained. A saved runtime list may
retain the marker; uninstall by restoring the prior mod files and a save from
before v5. Do not delete the marker quest from an ongoing save's plugin.

Log: `Documents/My Games/Skyrim Special Edition/SKSE/NoJarrinPlanting.log`.
After loading, `VERIFIED` means native list inspection found no Jarrin Root in
the Hearthfire seed list. This is a diagnostic check, not an in-game test result.

Build using `tools/build_windows.ps1` with Visual Studio 2022 and CMake.
Dependencies are pinned in that script and vcpkg.json. The workflow compiles a
Windows x64 DLL and runs slot-policy regression checks. Gameplay is untested.
