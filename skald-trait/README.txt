BIGGIE TRAITS COMBINED v2.6.0 - SKALD REBUILD

INSTALL
Close Skyrim. Replace your existing combined mod with this complete archive
in Mod Organizer 2. Ensure its ESP, BiggieTraitMechanics.dll and
BT_SkaldHitEffect.pex win file conflicts. Keep only one combined ESP enabled.
Skyrim Steam 1.6.1170, matching SKSE, Address Library, original Biggie Traits
and your existing combined-mod dependencies remain required.

USING SKALD
Select Skald in your trait menu. If already selected, it refreshes on load.
You receive the lesser power "Skald - Store Shout". Cast it, choose a shout
from the paged list, and return to combat. Only learned shouts with their
first word unlocked are listed. Clear stored shout disables the proc until
you store another. Removing Skald removes the power and disables its proc.

A melee power attack releases the stored shout's FIRST-WORD spell. A hit
is not required: missed swings count. Bashes, light attacks and ranged attacks
do not trigger it. Unarmed melee power attacks also qualify.

Base Speech below 50: 10-second recovery.
Base Speech 50 through 99: 6-second recovery.
Base Speech 100 or higher: 3-second recovery.
Temporary Speech bonuses do not change this threshold. The recovery is set
when the proc fires, uses unpaused gameplay time, and is saved in the SKSE
co-save. Changing the stored shout, clearing it or reselecting the trait
does not reset the remaining recovery. Keep the matching .skse co-save when
moving your saves. A missing stored shout is safely disabled after loading.

The drawback remains: light melee attacks deal 25% less physical damage.
The old effect that reduced normal shout recovery by 5 seconds on a hit is
removed. Skald neither spends nor resets your normal shout cooldown.

ECHOING STEEL
Skald's released shout arms Echoing Steel for the NEXT melee power attack
within 5 seconds: +50% physical damage, or +100% with a two-handed weapon.
It does not boost the same attack that released the shout. Example:
1. Power attack A releases Skald's first word and arms Echoing Steel.
2. Power attack B within 5 seconds consumes the damage bonus.
3. If Skald has recovered when B starts, B also releases a first word and
   arms a fresh bonus for attack C. Otherwise B only consumes its bonus.
Multiple contacts from an attack share its original damage multiplier.
A missed empowered power attack spends its bonus. Bonuses refresh rather
than stack. Echoing Steel's existing +25% power-attack stamina cost remains.
Normal player shouts continue to arm Echoing Steel as before.

COMPATIBILITY AND VALIDATION
The helper uses the first-word spell from the currently loaded shout record,
including spell/effect overrides. It uses instant casting while the attack
animation continues. Standard aimed shouts fire forward; self shouts affect
the player. No simulated VoiceFire event is broadcast to other mods.
Custom shouts that need normal voice animation/events, later words, or
concentration casting may need an additional patch. Custom actor-targeted
first-word spells require a crosshair target. Voice animation/audio and
combat-animation mod combinations need an in-game check.

Windows compilation and automated tests are required before this package
is produced. Record checks preserve Skald's drawback, all existing FormIDs,
all masters, the other nine traits' records, thumbnails and VenomHarvester.dll.
Two private records are added for the lesser power. The retired mark script
is compiled with empty OnHit and OnEffectStart handlers for existing saves.
The package has not been tested inside Skyrim here.

IN-GAME CHECK
Store Unrelenting Force with its first word unlocked. Power attack at a
target: check that the first-word effect fires once, with no repeat until
Skald recovers. Repeat at base Speech 49, 50 and 100. Save and reload, then
confirm the stored shout and remaining recovery. With Echoing Steel selected,
compare attack A and the following attack B against the same target. Try a
two-handed weapon, dual wielding and a missed attack. Remove Skald and confirm
its power disappears. If a case fails, provide BiggieTraitMechanics.log from
that session and the name of the selected shout and combat animation mod.

BUILD / SOURCE
Native helper source: three-traits/native in sl168898/Physics-helper,
branch codex/three-combat-traits. Build with its pinned Windows build script.
Papyrus source and its compiled compatibility stub are included. The stored
base64 copy is the exact compiled PEX; PEX-Inspection.txt records both empty
event handlers. rebuild.py validates native source hashes and packages the
complete v2.5.0 baseline. Run with --verify-plugin-only to check ESP changes
before supplying the native Windows artifact.

----- Historical notes below describe earlier versions; the rules above supersede old Skald rules -----

