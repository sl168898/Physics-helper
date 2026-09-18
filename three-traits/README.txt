BIGGIE TRAITS COMBINED v2.4

Three new traits, included with the previous seven in the same ESP.

BURDEN OF DEVOTION
Shrine blessings received while carrying no more than half your current
carrying capacity last twice as long and have 25% stronger numeric effects.
Carrying capacity is reduced by 50 while the trait is selected.
The threshold uses your capacity AFTER that penalty and other modifiers.
Example: 200 capacity becomes 150; carry 75 or less when receiving a blessing.
The bonus is decided when the blessing is received. Picking up more items
later does not rewrite that blessing. Previously active blessings are unchanged.
Recognizes the vanilla MagicBlessing keyword used by Wintersun and Reqtificated.
Instant cures and effects without a numeric magnitude do not gain extra potency.

UNBROKEN GUARD
Block three physical attacks within a rolling five-second window to charge
your next bash. That bash deals 5x damage (not +500%). Only one charge can
be held; further blocks do not bank charges. A missed bash spends the charge.
Unblocked physical hits against you deal 15% more damage. Actual hit block
flags are checked: holding a shield without blocking the hit is insufficient.
The penalty covers ordinary weapon hits, including arrows, not spell damage,
poison ticks, separately applied weapon enchantments or falling damage.

ECHOING STEEL
Using a shout arms your next melee power attack for five seconds.
It deals 50% more physical damage, or 100% more with a two-handed weapon.
A missed power attack spends the opportunity; ordinary attacks do not.
Only shouts trigger the bonus, not lesser powers. Repeated shouts refresh
the window rather than stacking bonuses. All power attacks cost 25% more
Stamina while the trait is selected, using the native perk entry point.

COMBAT TIMING
Five-second windows use player simulation time; paused menus do not spend it.
Multiple contacts in one empowered swing share its bonus. The following
attack receives no bonus. Unused charges/history clear on death or loading
a save. Removing the corresponding trait disables its mechanics.
Bonuses can combine with other traits, including Skald and Skull Rattler.

INSTALL
Replace the previous combined main package in MO2, then fully restart Skyrim.
Keep Biggie Traits - Combined.esp enabled with the existing dependencies.
Select the new traits through the normal Biggie Traits menu.
The included BiggieTraitMechanics.dll requires Skyrim 1.6.1170, SKSE and
Address Library. No extra plugin slot, new master, console setup or new game
is introduced. The existing VenomHarvester.dll remains included unchanged.
All seven earlier traits retain their form IDs, scripts and fixed thumbnails.

VALIDATION AND IN-GAME CHECK
Windows compilation and native rule tests passed. All previous ESP records,
scripts, DLLs and thumbnails were checked for byte-for-byte preservation.
The new DDS files use the corrected header already working in this package.
This update has not been tested inside Skyrim.
Suggested check: compare shrine effects at the weight threshold and above it;
block three attacks then bash twice; shout and compare one-handed/two-handed
power attacks inside and outside five seconds; verify both permanent penalties.
The SKSE log is BiggieTraitMechanics.log and should report all three traits ready.

Previous release notes below are historical:

