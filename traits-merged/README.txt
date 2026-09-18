BIGGIE TRAITS COMBINED v2.0 -- SINGLE ESP
Voice of Authority + Fully Devoted + Venom Harvester + Skald

This edition replaces the four addon ESPs with one ESL-flagged plugin:
Biggie Traits - Combined.esp
All four trait thumbnails and the v1.6 gameplay rules are preserved.

SAVE COMPATIBILITY
Use a new game, or a save made before installing any of the four original
trait addons. This is NOT a direct replacement on a character that already
uses the four plugins. Their forms now belong to a different plugin; saved
traits, quest aliases, globals and native poison history cannot simply
follow those changed identities. This package does not convert existing
saves. Keep the four-plugin v1.6 edition for those characters. Removing
traits or cleaning a save does not provide a verified migration path here.

INSTALL IN MOD ORGANIZER 2
1. Use a separate MO2 profile for your new game if retaining an existing
   character on v1.6. Disable the old combined addon in the new profile.
2. Install this ZIP as a mod and enable it in the left pane.
3. Enable Biggie Traits - Combined.esp. Keep these four addons disabled:
   Biggie Traits - Greybeard Trained.esp
   Biggie Traits - Fully Devoted.esp
   Biggie Traits - Devoted Alchemist.esp
   Biggie Traits - Skald.esp
4. Keep the ORIGINAL Biggie Traits mod and all its requirements enabled,
   along with your existing Requiem, Wintersun, Wintersun Reqtificated,
   Tribunal Integration and White Phial setup. Only the four custom trait
   addons are merged; the original gameplay mods remain separate masters.
5. Load the merged ESP after its masters and Biggie Traits - Requiem patch.
   Let this package's WSN_TrackerQuest_Quest.pex and FD_PowerGrantAlias.pex
   win script conflicts, including against Tribunal Integration.
6. Launch through SKSE and start the intended fresh game/save.

REQUIREMENTS
Skyrim 1.6.1170, matching SKSE, AE Address Library, and the existing Biggie
Traits requirements including FormList Manipulator, Keyword Item
Distributor and EditorID support. The native VenomHarvester.dll is updated
to 1.1.0 for the single-plugin layout. No extra ESP or new inventory item is
added by the native backend. The old addon ESPs are not masters of this file.

TRAITS
Voice of Authority: choose one full three-word starting shout from Predator's
Might, Fire Breath, Frost Breath or Kyne's Peace. Shout magnitude +1% per whole
point of base Speech, capped at +100%. Spell damage -35%. No clothing gifts.

Fully Devoted: automatic hard favor cap of 400%, with -25 percentage points
magic resistance. No activation power, starting favor grant or deity lock.

Venom Harvester: killing an enemy while your poison is active recovers one
bottle of the most recently applied poison still affecting them. One harvest
per enemy, including across save/load and resurrection. Applied poison
magnitude is multiplied by 0.75; duration-only effects instead use 0.75
duration. Instant poison qualifies only during its lethal application.
The White Phial returns an ordinary bottle of its selected poison, never a
second phial. The selected liquid is recorded when applied. No crafting
bonus, disease weakness, Jarrin Root gift or empty phial gift.

Skald: hostile melee power-attack hits reduce an active shout cooldown by
5 seconds, clamped at zero. Light melee physical damage -25%. Power attacks
keep their damage. No starting shout. Vokrii is not required.

WHAT WAS MERGED
34 private records, all four trait registrations and the shout-damage keyword
distribution. New record ranges: Voice 0x800-0x805; Fully Devoted 0x900-0x934;
Venom Harvester 0xA00-0xA06; Skald 0xB00-0xB05. Gaps are intentional.
Internal EditorIDs, script class names and thumbnail paths are preserved.
Script properties, conditions, perk references, quest aliases and the SEQ
startup entry have been remapped. Seven Papyrus lookups now use the merged
plugin. The native backend resolves Venom Harvester at local ID 0xA00.
Do not rename the merged ESP: the scripts and native DLL use its exact name.

VALIDATION AND FIRST TEST
The original gameplay data was compared with the merged records. Both
affected Papyrus scripts were compiled and their instructions compared
against v1.6 after reversing only the lookup edits. All four DDS thumbnails
and five unaffected compiled scripts remain byte-identical. The new native
DLL is built on Windows and checked against its source hashes.
No running Skyrim instance was available; in-game testing is still needed.

In your fresh game, check that all four traits appear with thumbnails. Test
the starting shout choice, Fully Devoted's cap/weakness, Skald's cooldown and
light-attack penalty, and Venom Harvester's recovery from a freshly poisoned
enemy. Its log should say "Trait layout: merged". Keep each save's matching
.skse co-save for native poison history. Detailed build and reference reports
are in Documentation.
