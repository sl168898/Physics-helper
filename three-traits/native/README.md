# Biggie Trait Mechanics 1.6.1 — Arcane Dynamo charge correction

For Skyrim 1.6.1170 and Combined 2.10.10-beta1. The user's 1.6.0 log shows
12.75 Magicka payments and fire magnitude 35 -> 45.5 (70 -> 91 with Runic
Overdrive), on weapon 0001DDA2 / enchantment 00045C2D. Weapon charge also
decreased according to the in-game report. Source verification identifies the
old suppression call 34143+151 as ActorMagicCaster::Update, not SpellCast.
The SpellCast-only suppression scope therefore did not cover the weapon's
native release charge call. The original log did not directly trace that call.

The correction removes the fixed GetAssociatedResource call-site patches and
delivery receipts. It scopes native CalculateCost to zero for the exact player
and weapon enchantment during CheckCast, SpellCast and FindTargets. Full native
CheckCast/SpellCast entry hooks also cover calls to the captured entry functions;
their addresses come from verified vtable slots 0A/09 before the Iron Lungs
wrapper is installed. Vtable wrappers retain other handlers, including Iron
Lungs. Nested matching wrappers do not run the
Magicka gate twice. Instant/other casters can match the exact equipped weapon.

Dynamo's own cost probe explicitly reads the original engine cost, including
perks, so the 15 base Magicka normalization is unchanged. Outside managed casts,
CalculateCost remains untouched: it is also used for gold values. No charge bar,
inventory data, enchantment record, or other actor is overwritten or refilled.
The existing delivery reservation, no-target refund, damage and regeneration
rules remain intact. All other trait implementations are unchanged.

The new regression suite covers release before/after delivery, original-cost
payment, insufficient Magicka, zero charge, nested enchantments, and shared-form
NPC isolation. These are logic tests, not execution of Skyrim. In-game testing
is still required, including empty-charge weapons and the user's load order.

The first 120 charge diagnostics per load include CHECK, RELEASE with both
hands' charge before/after, CHARGE COST SUPPRESSED, and UNSCOPED COST FORWARDED.
An unscoped query may be a normal inventory/value lookup; its presence alone
does not prove a failed charge bypass. Existing payment/damage logs remain.

API references: CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a,
MagicItem.cpp / Offsets.h (CalculateCost 11213/11321), MagicCaster.cpp
(FindTargets 33632/34410), and ActorMagicCaster.h (CheckCast/SpellCast ABIs).
Constellations f136590faf03efca649a98ea3b32dc1080d08e98, src/RE/Offset.h,
identifies 34143 as ActorMagicCaster::Update. The old ProjectStaff-derived
34143+151 hook was misattributed to SpellCast. Neither that ID nor those
instruction offsets are used for charge handling. BakaBloodMagic
10aa95c56244aff3f1c78c5584968e8a9f827341
documents release cost -> CalculateMagickaCost in ActorMagicCaster.h / Utils.h.

# Earlier Biggie Trait Mechanics 1.5.0 — Runic Overdrive

Requires Skyrim 1.6.1170, SKSE and Address Library, plus the matching Combined
2.9.0 ESP. This DLL includes all existing 1.4.5 trait behavior.

The new trait doubles native contact weapon-enchantment magnitude on melee
power-attack hits. Native duration-only effects (paralysis, soul trap,
invisibility and etherealize) double duration instead. An effect with damage
and duration gets only doubled magnitude. The ESP multiplies power-attack
Stamina cost by 1.30; misses and unenchanted power attacks pay the drawback.
Weapon-charge rules, proc chances and target immunities remain native.

Both effects on a multi-enchantment weapon qualify independently. This is a
per-application runtime adjustment; the shared enchantment definition and the
inventory copy are never rewritten. Spells, poisons, armor, staves, arrows,
bashes, NPC attacks and Skald's released shout are filtered out.

Runemaster compatibility audit: its ordinary added weapon enchantment follows
this trait. The custom rune carriers invoke separate delayed Papyrus spells,
explosions and hazards. Their scripts ignore the carrier magnitude, so those
scripted procs retain their original strength. No rune script, timer, cooldown,
visual, bridge DLL or Runemaster record is changed. Other enchantments with
script-controlled behavior also require specific integration and are not
advertised as generically doubled.

Implementation: native hit context supplies attacker, target, weapon and power
flag. The effect-adjustment call in MagicTarget::CheckAddEffect is chained
(34547 + 0x656, the three-argument ABI used by powerof3/MagicSneakAttacks at
37fe09931b9b1f1bd753da508132742d0f78aa33). Native contact enchantments processed
before the physical hit use the current active attack flags and exact equipped
source weapon; there is no elapsed-time buff. Nested physical hits replace and
restore their context. The effect is adjusted after normal engine scaling.
The shared trampoline is allocated once for Iron Lungs and Runic Overdrive.

Validation: unit tests cover magnitude vs duration, damage-over-time not being
quadrupled, script/hazard exclusion, invalid magnitudes, nested hits, other
weapons/targets, and trait/attack eligibility. Existing trait tests run too.
The Windows build and source hashes are recorded in BuildInfo.json. Actual
in-game timing, animation-mod interactions, dual-wield combos and resistances
still require testing in Skyrim. This is a beta, not a claim of live testing.

The first 64 qualifying effect adjustments per loaded session are logged as
[RunicOverdrive] BOOST or UNSCALED in BiggieTraitMechanics.log. No pop-up spam.

# BiggieTraitMechanics 1.4.5 — Iron Lungs projectile damage association

The submitted 1.4.3 log records 20 paid manual Unrelenting Force casts and
correct Stamina snapshots (420 gives 105 base magic bonus), but zero HIT lines.
Every FIND event belongs to the player at release; the later projectile impact
does not follow that assumed FindTargets path. Thus the bonus never reached
the damage routine in that session. 1.4.4 retained the same damage path.

The native helper now associates the exact projectile handle and original
spell at the explicit projectile magic-impact call. A scoped context carries
that cast's snapshot, spell, player caster and specific target through the
original function. Accepted AddTarget calls must match all those identities.
Untracked nested impacts clear the outer context; exiting restores it. Multiple
effects/projectiles share the cast's existing per-target deduplication. No
latest-cast timer, nearby-actor search or direct Health subtraction is used.

The obsolete FindTargets hook is removed. The impact call is AE REL44206+0x218;
the original eight arguments, including the final two bytes, are forwarded.
An unexpected call opcode disables Iron Lungs rather than patching unknown
instructions. Source evidence:
- AE callsite: NoahBoddie/perk-entry-expansion commit 2a75ca5d3d322ff4d60ba68dd691df8efb988fc5,
  src/MagicApplyCombatSpell/Hooks/Hook_MagicApplyCombatSpell.h.
- Complete eight-argument signature: Newrite/ReflyemSKSEPlugin commit
  f627a0ca47a0c4c9497f9c5ede5f01ac98d0ac44, include/Hooks.hpp (OnMagicHit).
- Independent byte-argument declaration: D7ry/valhallaCombat commit
  48fb4c3b9bb6bbaa691ce41dbd33f096b74c07e3, src/include/Hooks.h (Hook_MagicHit).

The newly reachable damage routine also corrects the incoming-magnitude perk
call's argument list: (target, spell, output), without an extra caster argument.
Outgoing magnitude still uses (caster, spell, target, output). Reflyem's
src/plugin/ProjectileBlock.cpp at the commit above corroborates both signatures.
This fixes parameter forwarding; the intended perk scaling is unchanged.

PROJECTILE, IMPACT, APPLY and HIT diagnostics trace the transaction into the
native bonus magic effect. HIT reports requested pre-resistance magnitude;
an accepted APPLY is not a direct measurement of final target Health loss.
The helper receives an explicit magnitude override after modifiers are evaluated
against the original UF spell. The old source comment attributing guaranteed
once-only perk scaling to Power Affects Magnitude being off is not established:
that flag alone does not prove that later perk adjustments are skipped. Final
scaling still requires a controlled in-game comparison; APPLY records do not
establish the final active-effect magnitude.
Seven rule suites and Windows compilation are required, but only an in-game
test can establish hook reachability and actual damage in the user's load order.
Test all three word lengths, several enemies, rapid repeated manual casts,
blocked/absorbed spells and Skald's automatic UF exemption.

All balance rules remain unchanged: 25% pre-payment current Stamina magic
damage, original UF perk scaling, native magic resistance/absorption, Stamina
cost/gate and recovery handling, word grant and three-second physical drawback.
Skald's 1.4.4 timing fix and Echoing Steel remain intact. The whole ESP, scripts,
INIs and artwork are copied byte-for-byte from combined 2.7.4-beta1.

## Earlier Skald self-buff release timing (1.4.4)

The supplied 1.4.3 log shows Kyne's Grace's actual Stamina effect at magnitude
30.25. Resource changes fit capped restoration followed by attack expenditure:
104.264435 + 30.25 - 69.525597 = 64.988838, versus 64.987730 observed. At a full
bar, that restoration is wasted before the attack charge. This supports a timing
problem, rather than a missing effect. Health and Magicka were full in the log.
Why this load order scales the original magnitude 100 to 30.25 remains unknown;
this update preserves the engine's magnitude calculations and adds no refund.

SKSE Hooks_Handlers.cpp sends WeaponSwing before calling the original handler.
Skald now reserves a self-delivery first-word release at that event and casts it
once from tick, after the containing original PlayerUpdate returns. Aimed and
other targeted shouts retain immediate release. No arbitrary timer, task-loop
polling, duplicate cast, manual resource change or synthetic VoiceFire is added.

The reservation stores form IDs, clears before casting and revalidates the
original known first-word shout. Loading, trait loss, death, killmove or changing
the stored shout cancels a pending release. Paused menus hold it. Pending casts
are transient and are not serialized. Cooldown and Echoing Steel are reserved
once at the original attack, preserving current-swing snapshots and following-
attack/other-hand behavior. An aborted pending cast does not refund that recovery.
Skald's casting guard encloses the actual release, preserving Iron Lungs exemption.

[SkaldBuff] QUEUED and RELEASE lines report Stamina at both boundaries; existing
BEFORE/IMMEDIATE/AFTER_UPDATE probes then show the effect application. These
observations remain read-only. Test with missing Stamina and compare ordinary
first-word casts; this change does not guarantee a net gain if attack cost is
greater than the restoration. Third-party delayed Stamina costs still need live
verification. Windows compilation and six test suites are required. Tests cover
release ordering, duplicate rejection, cancellation and Echo preservation, not
Skyrim's native resource processing. The complete ESP and assets remain unchanged.

## Earlier Skald cast-mode correction (1.4.3)

Skald now passes false for CastSpellImmediate's second argument, matching normal
fresh-cast implementations in PayloadInterpreter and PapyrusExtender. The old
true flag suppressed hit-effect art; another engine reference names it loadCast.
This is a targeted correction, not proof that every modded self-buff failure is
resolved. The exact engine branch responsible for the reported missing Kyne's
Peace restoration has not been observed here. Keep magnitude override at zero:
it preserves each effect's own magnitude, rather than forcing all effects to
one value. Original effects, conditions, scripts, durations and perk scaling
remain under the game's control. No direct actor-value restoration or fallback
recast is added, so healing and buffs cannot accidentally be granted twice.

All self-delivery stored shouts use the player target, including Kyne's Peace,
Predator's Might and Dragon Aspect when its loaded spell uses self delivery.
The exact first-word spell is used. Targeted/aimed shouts keep their old target
selection, independent 10/6/3-second recovery and next-attack Echoing Steel rules.
No manual VoiceFire event is fabricated, so Iron Lungs remains exempt for Skald.

Read-only [SkaldBuff] diagnostics record the actual loaded first-word effects,
Health/Stamina/Magicka before and immediately after casting, then after 0.25
unpaused gameplay seconds. Matching active effects include magnitude, duration,
inactive and dispelled state. The log is capped at 200 lines per loaded session;
reload to start another capture. Actor values may include regeneration and other
mods, and instantaneous healing may leave no active-effect instance. A matching
instance alone does not prove restoration succeeded. The probes do not change
stats, reapply spells or remove buffs.

Offline audit covers the supplied Requiem spell overrides: Kyne's Peace,
Predator's Might, Elemental Fury, Become Ethereal, Slow Time, Aura Whisper and
Clear Skies. Dragon Aspect's Dragonborn.esm records and winning overrides were
not supplied; the generic runtime capture will identify them in game. Five rule
suites and Windows compilation are build gates; actual buffs still need testing.
The entire ESP, all artwork, scripts and INIs remain unchanged from 2.7.2-beta1.

## Earlier Iron Lungs Overexertion (1.4.2)

While Iron Lungs is selected, a manual shout causes 20% extra incoming physical
damage for three gameplay seconds. Another manual shout refreshes the duration;
the amount and duration never stack. Paused menus freeze the timer. Skald's
automatic casts are exempt. The transient window clears on trait removal,
death, loading a save or starting a new game, like the existing combat tokens.

Manual Unrelenting Force arms the drawback only after its existing transaction
confirms a projectile launch; an unaffordable or refunded UF cast does not arm
it. Other shouts use the player's normal SKSE VoiceFire release event. This
event precedes native release processing, so a downstream release failure from
another mod can still arm the window. Spells and lesser powers do not qualify.

The existing game-thread Actor::ProcessHitData hook adds 20% of the positive
physical component (clamped to total hit damage) to physical and total damage.
Melee, arrows, unarmed attacks and bashes qualify. Magic/poison effects and any
nonphysical hit remainder are unchanged; zero-damage blocks stay zero. Existing
Guard/Echoing Steel multipliers run first and retain their original behavior.
Bounded [IronLungs] logs record incoming physical adjustments for live diagnosis.

Stamina cost, magic bonus, cooldown handling, the three-word grant and Skald
mechanics are unchanged. No global recovery penalty or added Stamina cost.
Five native rule suites, Windows compilation and package regression checks
are required. Live Skyrim testing is still required.

## Earlier full-shout grant (1.4.1)

Choosing Iron Lungs now adds Unrelenting Force and teaches/unlocks its three
words for free. Existing saves already using the trait synchronize after loading
and leaving paused menus. The existing per-frame player update detects trait
selection; the grant uses native Actor::AddShout and sequential vanilla Game
IsWordUnlocked / TeachWord / UnlockWord calls through the Papyrus VM. Already
learned and unlocked words are skipped, and each grant is verified. Failed
dispatches retry after five gameplay seconds. Load/removal epochs discard stale
continuations. A stalled VM callback can retry after thirty gameplay seconds.

Learned words remain if the trait is removed. Dragon souls, main-quest stages,
the equipped shout and all combat rules remain unchanged. No new Papyrus script
or extra plugin is required. The first completed sync logs all three words as
verified; this build still requires testing in Skyrim.

## Earlier Iron Lungs mechanics (1.4.0)

Adds Iron Lungs to the combined package for Skyrim Steam 1.6.1170.
The existing ten traits and Skald/Echoing Steel attack sequence are preserved.

Manual Unrelenting Force costs maximum Stamina times max(0.10, 0.25 times
ShoutRecoveryMult), adds non-elemental magic damage equal to 25% of current
Stamina immediately before payment, and produces no new shout recovery.
The current maximum includes Fortify Stamina. Voice of Authority's +20%
recovery penalty increases the cost; percentage recovery reductions lower it.
All three word levels use the same cost and bonus formula. Their existing
base damage, stagger, knockback, and other winning-override effects remain.

ActorMagicCaster::CheckCast rejects unaffordable manual casts. A second
check wraps VoiceSpellFireHandler::ExecuteHandler before the release occurs.
One payment is reserved per release, and refunded if no UF projectile launches.
Projectile handles retain separate cast snapshots; targets receive one bonus
per cast even if several original effects or projectiles touch them.
MagicCaster::FindTargets supplies projectile context to MagicTarget::AddTarget.
Only accepted original hits qualify. A target-actor helper spell applies the
bonus through the engine's Health-damage magic-effect path, with ResistMagic
and no elemental resistance or armor check. The engine's outgoing and incoming
spell-magnitude perk entry points are evaluated against the original UF spell.
The helper effect has Power Affects Magnitude disabled to avoid applying those
modifiers a second time. No Destruction skill experience is generated.

Skald's instant casts do not pass through the manual voice release handler,
so they incur neither this cost nor bonus. Its existing recovery and Echoing
Steel interaction remain. Normal manual VoiceFire events are retained.
No global shout cooldown multiplier, original shout, or original spell is edited.
No added regeneration penalty. Per-cast state is transient and reset on load;
a projectile already in flight when a save is reloaded has no retained bonus.

## Validation limits

This beta must pass Windows compilation, the four rule suites, and package
record/asset regression checks. These do not substitute for testing inside
Skyrim. The native cast and hit routes, absorption/resistance and interactions
with the user's animation/casting mods still require live verification.
The helper log records up to 200 Iron Lungs events per loaded session.

## Engine references

- CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a:
  ActorMagicCaster, VoiceSpellFireHandler, Projectile::Launch,
  MagicCaster::FindTargets, MagicTarget::AddTargetData, ActorValueOwner.
- powerof3/IndividualNPCShoutCooldowns e0dedfa21adad646eb9c8baa9afea536cf725cc9:
  native VoiceSpellFireHandler callback and voiceRecoveryTime location.
- tiltedphoques/TiltedEvolution fbf72883015dba9e26bd1539d3af9d33fa794116:
  MagicTarget::AddTarget AE address 34526 and FindTargets address 34410.
- Exit-9B/Constellations f136590faf03efca649a98ea3b32dc1080d08e98:
  outgoing/incoming spell-magnitude entry-point argument signature.

## Earlier source documentation

# Biggie Trait Mechanics

SKSE helper 1.3.2 for Skyrim 1.6.1170 and Biggie Traits Combined v2.6.2.

1.3.2 adds a bounded Echoing Steel diagnostic capture. The shipped
SKSE/Plugins/BiggieTraitMechanics.ini enables it; set Enabled=0 and restart
Skyrim to disable both tracing and diagnostic notifications. With no INI,
diagnostics are off. No combat rules, ESP records or saved data formats change.

The capture records trait selection, normal VoiceFire events, Skald arming,
attack phases, attack flags, bonus consumption/expiry, and outgoing melee
HitData immediately before/after the plugin's damage multiplier. It stops
after 200 records per save load. Existing callbacks are used: there is no
new polling script, timer or event listener. Optional notifications say
"primed" on arming and "bonus applied to hit" only when this damage hook
multiplies a positive, finite outgoing power-attack hit. The hit message is
shown once per tracked swing. These verify this plugin's damage change;
they do not measure final enemy HP loss after other mods and engine rules.

IN-GAME DIAGNOSTIC CHECK (not performed in the build environment):
1. Install the full combined archive, with its DLL and INI winning MO2 file
   conflicts. Start Skyrim and load your save. Keep Echoing Steel selected.
2. Manually shout, then land a melee power attack within 5 seconds. Look for
   "primed" and "bonus applied to hit". Repeat with a two-handed weapon.
3. With Skald selected and a stored shout, wait for its recovery, then make
   a power attack to release that shout. Land the FOLLOWING power attack
   within 5 seconds. This second attack should receive Echoing Steel.
4. Exit Skyrim and copy Documents/My Games/Skyrim Special Edition/SKSE/
   BiggieTraitMechanics.log BEFORE restarting. Its first line must say 1.3.2.
   A successful HIT record has echo_applied=true and multiplier=1.50 or 2.00.

The earlier user log contains startup lines only, so it cannot establish
whether Echoing Steel affected damage. The existing and expanded pure rule
tests pass; this release provides runtime evidence rather than asserting an
unobserved gameplay failure was fixed. The real game still needs the check.

Handles rolling block counts, single-attack tokens after blocks/shouts, and
Lab Skeever activation. Burden of Devotion now uses an ESP incoming-spell
magnitude perk (1.5x, MagicBlessing only) and a -100 carry-capacity effect.
All previous blessing vtable hooks and weight/duration rules are removed.
After updating an existing save, remove/reselect Burden of Devotion once
through the trait menu, then obtain a fresh shrine blessing.
MinHook is linked statically; no additional DLL installation is needed.

1.3.1 corrects Skald's menu button mapping. The old code set MessageBoxData
offset 0x4C to 4; this is buttonPressOffset, so the game added 4 to every
selected button index. The result could select another shout, navigate,
clear/cancel, or be discarded instead of storing the clicked shout. The
offset is now explicitly zero, with an offset assertion for the pinned ABI.
The native warningType default is also retained (0x38 is not menu depth).
Menu opening, raw button indices, resolved actions and failed validation now
appear in BiggieTraitMechanics.log. A power attack without a stored shout
logs once per selection/load, so this failure is visible without log spam.
The attack/casting behavior, recovery, save format and all ESP records remain
unchanged. Cast logs say "requested" because the engine API has no result.
After replacing the package, use Store Shout again and check for the
"Skald stored: <name>" notification; a previous choice may have mapped wrongly.

MessageBoxData layout reference:
https://github.com/adya/CommonLibSSE/blob/3adc3270274f954caebc165ddcc7a3969596eb1e/include/RE/M/MessageBoxData.h

Attack history and unused combat tokens reset on loading a save or dying.
No effect is enabled without its corresponding trait ability.
The native carry-weight penalty and power-attack stamina cost are in the ESP.
Original VenomHarvester.dll remains unchanged.

Skald grants "Skald - Store Shout", a lesser power with a paged choice of
learned shouts whose first word is unlocked. Power attacks release the stored
shout's first-word spell; the independent recovery is 10 seconds, 6 at base
Speech 50, and 3 at base Speech 100. It uses gameplay time and pauses in menus.
The chosen FormID and remaining recovery are serialized in the SKSE co-save,
with load-order resolution and asynchronous unlocked-word validation on load.
Changing the choice or reselecting the trait does not reset recovery.
Removing Skald removes its power. The ESP retains its 25% light-melee penalty.
The old cloak and OnHit shout-cooldown-reduction script are retired; the
ability is refreshed automatically once on load or selection for old saves.

An attack consumes any existing Echoing Steel bonus before releasing Skald's
shout. That shout arms the NEXT melee power attack within 5 seconds. Repeated
contacts from the current swing cannot consume this newly armed bonus.
Normal shouted VoiceFire events still arm Echoing Steel. Skald explicitly
arms it once without sending a fabricated VoiceFire event to other mods.
Ordinary shout cooldown is neither spent nor reset by this proc.
Shout selection checks run only when choosing or loading; there is no
Papyrus polling timer. The native player update already used by the other
combat traits advances the recovery and detects attack phases.

First-word spells use the instant magic caster so the power attack animation
continues. Standard aimed shouts fire forward and self shouts affect the
player. A custom target-based spell needs a crosshair actor; concentration
spells cannot be emitted as instantaneous first words. Mods requiring the
normal shout animation, later words or a VoiceFire event may need additional
support. In-game effects, voice audio and combat-animation combinations have
not been tested here. The Windows build and rule tests are package gates.

Lab Skeever activation now uses native TESFurnitureEvent and Crafting Menu
open/close events. Alchemy furniture is identified by its actual workbench type
(5 or 6), not a keyword. The actual AlchemyMenu subtype provides another route.
One completed visit queues one game-thread cast of the existing 20-second
bonus spell. Duplicate exit events do not stack or double-refresh it. An old
visit cannot grant a bonus after loading another save or removing the trait.
The existing bonus perk still supplies 30x potion duration and 1.1x magnitude;
food remains excluded and its 0.5x duration drawback remains unchanged.
There are no notifications. BiggieTraitMechanics.log records detected lab
visits and the result of applying the bonus, for in-game diagnosis.

Technical sources:
- CommonLibSSE-NG b93280e832f263dbef44e44cbe2936622a02f91a (headers).
- Actor::ProcessHitData ID 37633/38586 and Actor*, HitData& signature:
  https://github.com/KrisV-777/Acheron/blob/ab9d303af7636ad6b25d04d9a61d82e497890249/src/Acheron/Hooks/Hooks.cpp
  https://github.com/D7ry/valhallaCombat/blob/48fb4c3b9bb6bbaa691ce41dbd33f096b74c07e3/src/include/Hooks.h
No external mod source is copied into this helper.


## Arcane Dynamo (1.6.0; Combined 2.10.0 beta)

Arcane Dynamo (SPEL F60, MGEF F61) replaces the resource cost of native touch weapon enchantments with Magicka. The baseline is 15 Magicka, multiplied by the native actor-dependent charge cost divided by the same enchantment's actor-independent cost. This preserves charge-use perks and buffs instead of imposing a fixed fee. Zero-cost enchantments remain free; invalid costs fail closed. Staff spells, armor, poisons and scripted rune explosions are outside this contact-enchantment path.

MagicCaster::FindTargets (33632/34410, pinned CommonLib ABI) reserves the cost once for the entire delivery, before Absorb Magicka can take effect. Zero-target deliveries refund their reservation. Insufficient Magicka suppresses only the enchantment delivery. A receipt ties native SpellCast resource suppression to the delivery; no inventory charge is restored, replenished or overwritten. CheckCast chains the previous handler (including Iron Lungs) with only the charge-resource query bypassed. AE resource call sites 34145+BE and 34143+151 follow ProjectStaff c2d9d4266724c09316f14e84d9db8b8d82e1d9bb. Opcode guards disable the feature if the expected call sites differ.

The effect-adjustment chain applies +30% to native Health/Magicka/Stamina damage and absorption magnitudes only. Other effects retain their values. Runic Overdrive's existing x2 therefore stacks to x2.6 on qualifying power attacks. No duration multiplication is added by Dynamo. The player's positive MagickaRate read is multiplied by .65, preserving other regeneration modifiers and avoiding permanent actor-value changes. No new drawback or rune scaling is added to Runic Overdrive.

Diagnostics: first 80 payments, refunds, failed payments and magnitude changes are logged as [ArcaneDynamo]. Required in-game acceptance: empty-charge weapon with sufficient Magicka; insufficient Magicka with unchanged physical damage; multi-effect weapon pays once; left/right hand; bow/crossbow native delivery; Absorb Magicka; enchanted Runemaster weapon; trait removal; save/load; existing Iron Lungs/Skald/Runic Overdrive. Windows compilation and rule tests do not replace these in-game checks. This version has not been tested in Skyrim.
