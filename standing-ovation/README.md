# Standing Ovation native component

For Skyrim Steam 1.6.1170, SKSE64 and Address Library (AE). Requires the matching updated BOOB.esp and performance scripts from the complete patch archive. This native build alone is not an installable update.

The two original inn reward calls pass their reward potions to the plugin. With Standing Ovation, supported beneficial Health, Magicka, Stamina, Speech and Barter amounts become private constant abilities at three times the source potion magnitude. Repeated performances replace the same instrument's stat bonus; different instrument stat bonuses coexist until the next town arrival. Other performances use the original reward.

A player location event tracks the nearest town/city/settlement with an inn. It uses LocTypeHabitationHasInn and scans inn location parent chains. Leaving an inn into its own town retains the bonus. Leaving town, then returning or entering another eligible town ends it. Save loads establish a baseline and are not arrivals. No polling loop, duration extension hack, or recurring timer.

The SKSE co-save stores the reward amounts and current town, resolves changed load-order IDs, and isolates state between saves. Keep the matching .skse file with each save. Native mutations run on the game thread; queued work is invalidated during loads.

Only positive stat modifiers passed from Positive1/Positive2 are supported. Unexpected rewards are retained through the original code path and logged. Kyne's Peace, ordinary potions, alcohol, and other perks are not converted. StandingOvation.log records readiness, reward amounts, and expiration.

Windows compilation and deterministic reward/transition/save-state tests are automated. Actual Skyrim gameplay still needs verification.

Source is MIT licensed. CommonLibSSE-NG has its own included license. The original BOOB and Skyrim's Got Talent assets are not part of this source repository.
