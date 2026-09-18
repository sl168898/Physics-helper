BIGGIE TRAITS COMBINED v1.6
Voice of Authority + Fully Devoted + Venom Harvester + Skald

INSTALL / UPDATE IN MO2
Close Skyrim. Replace the previous combined addon with this ZIP and keep all
four ESPs enabled. Do not keep an older standalone copy of these addons active.
- Biggie Traits - Greybeard Trained.esp (Voice of Authority)
- Biggie Traits - Fully Devoted.esp
- Biggie Traits - Devoted Alchemist.esp (now Venom Harvester)
- Biggie Traits - Skald.esp
The old ESP filenames are intentional and preserve existing save identities.

Keep Biggie Traits and its existing requirements, including FLM, KID and
EditorID support, plus your Requiem/Wintersun Reqtificated/Tribunal setup.
Keep The White Phial - Tweaks and Enhancements.esp; it remains an existing
master. Load the addons after their masters and the Greybeard-named addon
after your Biggie Traits - Requiem patch.

NEW REQUIREMENT FOR VENOM HARVESTER
Skyrim 1.6.1170, matching SKSE, and Address Library for SKSE Plugins (AE).
The archive includes SKSE/Plugins/VenomHarvester.dll. Launch Skyrim through
SKSE in MO2. The DLL intentionally supports only your specified runtime.
Let this combined package's scripts win in MO2, including
FD_PowerGrantAlias.pex and the existing WSN_TrackerQuest_Quest.pex.
Keep the Fully Devoted ESP enabled even if you do not choose Fully Devoted;
its existing player controller also performs the Venom Harvester upgrade.

VENOM HARVESTER -- REPLACES DEVOTED ALCHEMIST
When you kill an enemy while your poison is active, recover one bottle of
the most recently applied poison still affecting that enemy. One recovery
per enemy, including after saving, reloading or resurrecting the same corpse.
Your applied poisons are 25% weaker: magnitude x0.75, or duration x0.75 for
duration-only effects such as paralysis. No crafted-item or potion bonus.
The recovered bottle retains the original poison recipe and strength; the
weakness applies again when used, without permanently weakening the item.

Expired/dispelled poisons and kills credited to other actors do not qualify.
Instant poison qualifies only if its application kills the enemy. Essential
knockdowns do not count as kills. Normal potions, spells and enchantments
are unaffected. Mod scripts that deal hard-coded damage independently of
their effect magnitude/duration cannot be universally rescaled.

The White Phial returns one ordinary bottle of its selected poison, never
another phial. The selected liquid is captured when the poison is applied;
changing the phial later does not change that earlier target's reward.
An unrecognized/mismatched phial mapping grants no substitute reward.

UPGRADING FROM v1.5
An already selected Devoted Alchemist is automatically refreshed into Venom
Harvester shortly after loading. Its old crafting perk and disease weakness
are removed, and no further Jarrin Root or empty phial gifts are granted.
Previously received items remain in your inventory. A new game is not
required by the implementation. Start testing with a freshly poisoned enemy.
Keep each save's matching .skse co-save to retain poison history and the
one-recovery-per-enemy guard. No custom on-screen debug notifications.

VOICE OF AUTHORITY -- PRESERVED
Choose one full three-word starting shout: Predator's Might, Fire Breath,
Frost Breath or Kyne's Peace. Shout magnitude +1% per whole point of base
Speech, capped at +100%. Spell damage -35%. No clothing gifts or cooldown
change. Its ESP, scripts, settings and thumbnail are preserved from v1.5.

FULLY DEVOTED -- PRESERVED
Automatic hard favor cap of 400%, with -25 percentage points magic resistance.
No activation power or starting favor grant. Its ESP and Wintersun tracker
are preserved. The shared player controller only adds the poison-trait
migration and native work queue call; favor-cap handling remains unchanged.

SKALD -- PRESERVED
Hostile melee power-attack hits reduce an active shout cooldown by 5 seconds,
clamped at zero. Light melee attacks deal 25% less physical damage. Power
attacks keep their damage. No starting shout. ESP, script and thumbnail
preserved from v1.5. Vokrii is not required.

FIRST IN-GAME CHECK
Confirm Venom Harvester and the new thumbnail appear in the trait menu.
Use a known damage poison on an enemy, then kill them before it expires:
one bottle should be returned. Repeat with a brewed poison and your phial.
Check that an expired poison and a follower's kill give no bottle. With two
different active poisons, the most recently applied still-active one wins.
Save after poisoning an enemy, reload, then finish that enemy to check the
saved tracking. Re-killing the same resurrected enemy must give no second dose.

VALIDATION
Windows DLL compilation, native rule/serialization tests, compiled Papyrus,
ESP references, DDS format and archive checks are included in validation.
The other three traits' ESPs, effect scripts and thumbnails are unchanged.
No running Skyrim instance was available here: in-game testing is still
required, particularly with your combat and poison mods. If recovery fails,
send VenomHarvester.log from your SKSE log folder.
