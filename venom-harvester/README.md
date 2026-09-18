# Venom Harvester

Native backend for the Venom Harvester replacement in Biggie Traits Combined v1.6.
Requires Skyrim 1.6.1170, matching SKSE and Address Library for AE. Install the
complete combined archive; this DLL alone does not add a selectable trait.

When you kill an actor while one of your poisons is active, recover one bottle
of the most recently applied poison that still has a harmful effect active.
Each actor reference can pay only once, including across save/load or resurrection.
Applied poison magnitude is multiplied by 0.75; duration-only effects instead
receive 0.75 duration. Crafted and purchased base items are never edited.
Beneficial potion effects, NPC poisons, weapon enchantments and ordinary spells
are excluded. Arbitrary damage hard-coded inside another mod's Papyrus script
cannot be rescaled by modifying the effect's magnitude/duration.

The original White Phial mod's custom poison maps to its selected ordinary
poison at application time. Recovery never duplicates a phial or its refill
marker. A later liquid reassignment does not change an earlier target's reward.
Unrecognized or mismatched phial contents are refused rather than guessed.

The plugin hooks each concrete active-effect vtable using CommonLib's documented
AdjustForPerks, Start, Update, Finish and OnRemove virtual slots. It calls the
previous function in each slot. Poison ownership is checked through the caster,
target and source AlchemyItem. Current active effects and death events determine
eligibility. Instant poison only qualifies during its lethal native callback.
Deferred game-thread inventory changes require actual death; essential knockdowns
and canceled deaths do not pay. It keeps copied IDs instead of dangling effect
pointers after engine callbacks.

The SKSE co-save stores application ordering, original returned poison forms,
pending rewards and completed victim references. Its load path resolves FormIDs.
A private saved form list retains dynamic brewed poisons. Keep the matching
`.skse` co-save with each Skyrim save. With a missing co-save, the plugin cannot
recover old application history; start with a freshly poisoned target.

The combined package retains the legacy `Biggie Traits - Devoted Alchemist.esp`
filename, its internal trait IDs and thumbnail path for upgrades. A one-time
controller migration refreshes the selected ability, removes the retired crafting
perk and disease effect, and disables future starting gifts. Items already gifted
remain in inventory. Other combined traits retain their existing behavior.

Build: run `tools/build_windows.ps1` on Windows with Visual Studio 2022 C++ tools,
CMake, Git and PowerShell 7. Dependencies are pinned in the script. The build
compiles the actual DLL, runs native rule/serialization tests and records hashes
in BuildInfo.json. Source is in the `codex/venom-harvester` branch of
https://github.com/sl168898/Physics-helper.

Validation does not include a running Skyrim instance. In-game checks still
needed: ordinary damage poison, paralysis, a brewed multi-effect poison, two
different active poisons, expired/dispelled poison, non-player kills, save/reload,
the White Phial and reassigning its selected poison. Diagnostic output goes to
VenomHarvester.log in the SKSE log folder; no custom on-screen notifications.

API references (pinned CommonLib b93280e832f263dbef44e44cbe2936622a02f91a):
- include/RE/A/ActiveEffect.h
- include/RE/M/MagicTarget.h
- include/RE/T/TESDeathEvent.h
- include/RE/B/BGSListForm.h
- include/SKSE/Interfaces.h
