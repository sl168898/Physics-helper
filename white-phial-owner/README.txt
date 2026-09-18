BIGGIE TRAITS COMBINED v2.1 -- THE WHITE PHIAL OWNER
Single ESL-flagged ESP: Biggie Traits - Combined.esp

Adds The White Phial Owner as a fifth selectable trait, with a new thumbnail.
Voice of Authority, Fully Devoted, Venom Harvester and Skald are unchanged.

THE WHITE PHIAL OWNER
Receive one original empty White Phial on choosing the trait. While carrying
the phial, empty or filled, potions AND poisons you make are 25% stronger.
The White Phial shop is permanently closed to you. Its entrance requires a
key and player activation is blocked even if you possess the real shop key.
The acquisition/repair story quests are stopped without awarding completion.

The gift is once per save. If you already own a phial, it and its selected
liquid are preserved; no duplicate is created. Storing or losing the phial
turns off the crafting bonus and does not grant a replacement. Bringing it
back restores the bonus. Existing and purchased consumables are unchanged.
The bonus is checked by an alchemy perk when crafting, with normal game
scaling, stacking and display rounding. It does not multiply phial contents
merely because the phial refills or is decanted.

The shop ban is permanent after selecting the trait, including if the trait
is later removed. The normal inside exit is left usable. NPC activation is
allowed through the door's normal processing; its key and ownership remain.
The controller reasserts the lock after unlock events, cell reloads and
save/load, with a periodic fallback. It identifies entrances by their actual
teleport destination into the original White Phial shop cell, including
replacement entrances that still lead into that cell.

PHIAL FUNCTIONALITY
The gift uses The White Phial - Tweaks and Enhancements' existing empty item
and refill function. Its background MS12PostQuest service is started without
completing either story quest or calling a quest reward function. The first
refill uses the original healing essence if no liquid was previously chosen.
Your normal liquid reassignment, full-enchantment requirement, hotkey and
refill settings remain controlled by the original mod and your menu addon.
This trait does not grant full enchantment or a new lesser power. The decant
addon continues to apply its own full-enchantment requirement.

The ban does not undo quests/rewards already completed before choosing the
trait. Choose it before the White Phial story quests for the intended tradeoff.
The targeted story quest IDs are MS12 and MS12b; MS12PostQuest stays running.

INSTALL IN MOD ORGANIZER 2
1. Install this ZIP and use it in place of the previous single-ESP package.
   Keep only one copy of Biggie Traits - Combined.esp enabled.
2. Keep the original Biggie Traits mod, The White Phial - Tweaks and
   Enhancements, and the existing masters/dependencies enabled.
3. Keep SKSE and powerofthree's Papyrus Extender installed and working. The
   new scripts use GetDoorDestination from Papyrus Extender, which the
   original Biggie Traits/White Phial setup already uses.
4. Preserve the previous script conflict priority: this package's existing
   WSN_TrackerQuest_Quest.pex must still win against other Wintersun scripts.
5. Select The White Phial Owner through the ordinary Biggie Traits menu.

SAVE COMPATIBILITY
This is an additive update to single-ESP v2.0: all 34 previous records keep
their FormIDs and contents; all seven old PEX scripts and the native DLL are
byte-identical. Six new records use local IDs 0xC00-0xC05. The new trait's
effect starts its controller if the quest did not start automatically.
Updating an existing v2.0 save is designed to work but remains untested in game.

If you are still using the four-ESP v1.6 edition, the previous merge restriction
still applies: use a new game or a save from before installing those addons.
This ZIP does not migrate a four-ESP save into the single-ESP layout.

REQUIREMENTS AND EXISTING TRAITS
The inherited Venom Harvester native DLL still targets Skyrim 1.6.1170 and
requires the matching SKSE/AE Address Library. Existing Biggie Traits,
FormList Manipulator, Keyword Item Distributor, EditorID support, Requiem,
Wintersun and the other existing plugin masters are retained.

Voice of Authority: choose a full starting shout; shout power +1% per whole
base Speech point, capped at +100%; spell damage -35%; no clothing gifts.
Fully Devoted: automatic hard favor cap 400%; magic resistance -25 points;
no starting favor grant, activation power or deity lock.
Venom Harvester: one recovered ordinary poison bottle per poisoned victim;
applied poison magnitude/duration penalty 25%; White Phial liquid supported.
Skald: hostile melee power-attack hits reduce shout cooldown by 5 seconds;
light melee physical damage -25%; no starting shout.

VALIDATION
All four new Papyrus scripts compile and disassemble. The perk condition,
original phial list, shop cell, quest/alias bindings, SEQ, trait registrations,
DDS thumbnail and preservation of old records/scripts/assets were checked.
The new artwork was generated with the built-in image tool and converted to
1024x704 DXT5 DDS. Prompt and detailed validation are in Documentation.
There was no running Skyrim instance: in-game testing is still needed.

TEST IN GAME
Compare the same potion and poison recipes with the phial carried, stored,
and carried again; check both empty and filled states. Verify the gift refills,
is not repeated after save/load or reselection, and retains liquid changes.
Visit the shop in business hours and at night, wait and reload nearby, then
try its entrance with the shop key. Check the story quests stay unavailable.
Diagnostics use Papyrus traces prefixed [White Phial Owner], without popup
notifications. The usual item/menu messages from the original mod remain.
