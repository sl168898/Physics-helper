# Waterskin Bottle Return — SKSE v2 (DLL 1.1.0)

Target: Skyrim executable 1.6.1170, SKSE64 for that runtime, AE Address Library,
and the supplied Waterskin.esp, Requiem.esp and Personal Tweaks.esp.

Your existing cooking recipe remains unchanged:
1 carried Waterskin (Full) -> 3 Requiem Bottled Water + 1 original Waterskin (Empty).
No new items, ESP, quests or Papyrus scripts. Independent of the flail/FSMP DLL.

## Upgrade

1. Exit Skyrim. Replace the earlier Waterskin SKSE v1 mod with this package.
   Only this version of SKSE/Plugins/WaterskinBottleReturn.dll should be active.
2. Keep the earlier batch-item patch Waterskin_BottledWater_Return.esp disabled.
   Keep your original three plugins and use the original bottled-water recipe.
3. Launch through SKSE, open a cooking station and craft once.
   Expect -1 filled waterskin, +3 bottled waters, +1 empty waterskin.

For a first test, use a separate save. If you used the earlier batch-item ESP,
a save from before that patch avoids retaining its unused quest or batch items.
This native v1-to-v2 upgrade has no serialized state and does not require a new game.

## Change from v1

The user log showed the DLL loaded and the original recipe passed validation,
but there was no craft-result log. v2 removes the confirmation callback hook.
Instead it listens for native crafting-menu, container-change and ItemCrafted
events. The helper requires a bottled-water craft signal AND the matching removal
of a carried filled waterskin AND receipt of three bottled waters per skin.
It processes these notifications after the current operation, independently of
their order within that operation, and returns the original empty skin.

Cancellation, inventory additions outside crafting, and water recipes that do
not consume a carried filled skin do not qualify. Existing empty skins returned
by another helper in the same transaction count toward the return, avoiding a
duplicate. Ambiguous mixed crafting events fail closed. An original recipe whose
winning record is changed to another input/output is rejected.

The event counters are restricted to the crafting-menu session and cleared on
load/new game. There are no continuous polling timers, notifications, new forms,
or save serialization. It does not change the water or waterskin item records.

## Test and diagnosis

Test one craft, cancel one, craft several more, then save/reload and craft again.
Compilation and automated reward-rule checks passed. v2 still needs in-game
testing with your modlist. Ingredient consumption from remote containers and
mods which suppress native ItemCrafted events are not established as supported.

If the return is missing, exit Skyrim and upload WaterskinBottleReturn.log from
Documents/My Games/Skyrim Special Edition/SKSE. This version records menu opening,
each relevant inventory event, the craft signal, and the matched/unmatched result.
Expected startup: WaterskinBottleReturn 1.1.0 and registered event listeners.

Disable this native mod with Skyrim closed to remove it. Already awarded normal
items remain. BuildInfo.json identifies the exact source and Windows build.
