# Wheeler Refined — Enchantment Swapper bridge 1.1.0

Replacement wheeler.dll for the supplied Wheeler Refined 1.3.3.0 and Skyrim
Steam 1.6.1170. Install the complete Enchantment Swapper Description Fix 1.1.0
package below Wheeler, Wheeler Refined, and the earlier Rename Potions patch in
MO2. Keep the original Wheeler/Refined mods and their dependencies enabled.

Transferred weapon and armor descriptions use EnchantmentSwapperDescriptions.dll
API v1. The selected item's extra-list identity and current enchantment are
copied into numeric IDs; no borrowed inventory pointer crosses DLL boundaries.
The donor description replaces the receiving item's base/generic text when a
valid transferred description exists. Normal descriptions remain the fallback.
HTML is removed for Wheeler's plain-text renderer, while numeric markers and
live global values are resolved by the helper. This work happens only when
Wheeler draws the highlighted item; no additional background timer is installed.

The earlier Rename Potions compatibility changes are retained in this build.
Rename Potions itself is optional for enchantment-description compatibility.
If an existing wheel slot is still linked to the unenchanted item, remove it
and add the transferred item again. The description helper's existing 1.0 save
bindings are retained. No new game or repeated enchantment transfer is needed
for items that already have a saved description binding.

This is based on Wheeler Refined upstream revision
e3360bf81d05f739d2caa94d50e64e174b0ce1f8 and the previously delivered Rename
Potions patch. Complete modified source, build scripts, tests and notices are
included under Source and Licenses. The modified Wheeler build is GPL-3.0-only;
that license is separate from the description helper's MIT license.

Build and automated tests are verified separately from Skyrim gameplay.
The new bridge has not been tested inside Skyrim here. The Wheeler log should
show `Enchantment Swapper description bridge connected (API v1)`. The helper
log should show `Wheeler description` followed by the item/source IDs and text.
