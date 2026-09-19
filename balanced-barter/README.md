# Balanced Barter for SkyUI 6 — v0.1 test build

Build a pending exchange with items on both sides, then transfer only the gold
difference. Selecting an item does not immediately buy or sell it.

Example: your potion sells for 1,000, and the merchant's amulet costs 1,000.
You have no gold and the merchant has 100. The exchange costs no gold, and the
merchant keeps their 100. The numbers are the merchant's actual buying and
selling quotes, not the base values shown outside the barter menu. Speech,
Haggling, enchantments and ordinary price modifiers still affect those quotes.

## Requirements and installation

- Steam Skyrim Special Edition **1.6.1170**.
- Matching SKSE64 and Address Library (Anniversary Edition / 1.6.x).
- **SkyUI 6.11** installed normally. This addon contains one menu override and
  requires SkyUI's other files. Other SkyUI 6 releases have not been verified.
- Install this ZIP with MO2. Put it below SkyUI and any interface theme in MO2's
  left pane so its `Interface/bartermenu.swf` wins. There is no new ESP.
- Launch through SKSE. The barter menu should show YOU OFFER / YOU RECEIVE.

This first build is compiled and automatically checked, but **has not been
tested inside Skyrim**. Start with a separate save/profile and a small exchange
before using it in a continuing playthrough. Check the offered quantities,
gold, enchantment, charge and tempering after the first exchange.

## Controls

| Action | Mouse / keyboard | Xbox-style controller |
|---|---|---|
| Add item or quantity | Normal item selection | Normal item selection |
| Confirm complete exchange | Exchange button or Ctrl+Enter | X |
| Clear the entire offer | Clear button or Ctrl+Backspace | Y |
| Remove one offer entry | Click its row in the offer panel | Clear and rebuild the offer |
| Offer pages | Arrow buttons or Page Up / Page Down | LT / RT |
| Cancel and leave | Normal exit / Tab | B |

The offer panel displays four entries per side per page, up to 64 distinct
entries in one exchange. Add additional items to cover a shortage. If you offer
more than the merchant can pay, confirmation is blocked until you buy more,
remove some offered items, or choose a merchant with enough gold. Unpaid value
is never silently discarded. Leaving the menu discards an unconfirmed offer.

## Implementation and compatibility

- Uses SkyUI 6.11's barter source, with a new offer panel.
- Uses the quoted unit price at selection; no extra barter discount or mark-up.
- Validates quantities, live inventory instances, merchant identity and the net
  gold payment before committing. Changed inventory cancels the offer.
- Transfers existing inventory instances through the engine, including their
  enchantment, tempering, custom name, soul and charge data. Does not recreate
  equipment from base forms. Recovery attempts reverse completed transfers if
  another inventory hook prevents a move; a recovery failure is logged.
- No temporary gold grant, persistent credit account, power, quest or Papyrus
  script. Pending offers do not survive closing the menu or loading a save.
- Sends the total underlying value of all exchanged units, including enchanted
  value, through the engine's Speech skill-use function. The engine applies its
  skill-use multiplier and experience bonuses. Mods changing only the vanilla
  individual-transaction callback may still need a compatibility patch.
- Native inventory transfers replace the vanilla per-item barter callback.
  Mods hooking that callback, custom currencies, sale-specific scripts,
  unusual merchant containers and unpaused barter need separate game testing.
  Compatibility with those systems is **not established**.
- Other `bartermenu.swf` replacements need a combined patch; this addon replaces
  their barter layout. No claim of Untarnished UI / Dear Diary skin support.
  Other menus keep whichever interface files your mod setup supplies.
- English offer-panel labels. SkyUI's existing localized inventory UI remains.

If the DLL does not load, the offer interface blocks transactions and displays
an error. Review `Documents/My Games/Skyrim Special Edition/SKSE/BalancedBarter.log`
and `skse64.log`. Disable this addon to restore the underlying SkyUI barter menu.

## Build and verification

Pinned Windows build: VS2022, CommonLibSSE-NG, vcpkg and SkyUI source revisions
are recorded in `BuildInfo.json`. The build compiles the native DLL and the
modified ActionScript into a real SWF. Automated tests cover net settlement,
gold conservation, affordability, limits, quantities, stale reservations and
recovery on simulated item/payment failures. Those tests do not substitute for
Skyrim runtime tests, controller testing or visual checks in the game.

Sources: <https://github.com/sl168898/Physics-helper/tree/codex/balanced-barter-skyui6/balanced-barter>

SkyUI and dependency credits are in `UPSTREAM-NOTICES.md` and `licenses/`.
