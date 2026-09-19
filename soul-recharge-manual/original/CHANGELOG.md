# Changelog

## v1.1.0

- The Rare Curios Flawed Varla Stone and the Saints & Seducers Soul Tomato can now be used as automatic recharge fuel. The Flawed Varla Stone is reusable: it stays in your inventory and only the soul inside it is cleared. The Soul Tomato is an empty/filled pair, and the filled gem is consumed like any other soul gem.
- The extra gems are declared in the settings file, not in the MCM: one `ExtraForm=<plugin file name>:<local form ID>` row per gem under a new `[ExtraGems]` section. The shipped file contains the two Creation Club rows above commented out; remove the leading `;` to enable them. Capacity, contained soul, and the paired filled form are read from the plugin's own record, so only the identifier is written here.
- One bad row is skipped on its own and never disables the feature: the log records a status for every configured row, such as `RESOLVED`, `PLUGIN_ABSENT` (plugin not in the load order), `FORM_NOT_FOUND` (plugin present, form missing), or `MALFORMED` (a row that cannot be parsed).
- No changes to the ESP or to any save record, and no new save data. "Use soul gems" also covers the new gems. With the two rows left disabled as shipped, or with the `[ExtraGems]` section absent or empty, v1.1.0 behaves exactly as v1.0.0 did.

- Rare Curios の Flawed Varla Stone と Saints & Seducers の Soul Tomato を、自動充填の燃料として使えるようにした。Flawed Varla Stone は繰り返し使え、所持品から消えず中身の魂だけが空になる。Soul Tomato は「空/充填済み」の対で、充填済みのほうを通常の魂石と同じように消費する。
- 追加する魂石は MCM ではなく設定ファイルで指定する。新しい `[ExtraGems]` セクションに、1個につき `ExtraForm=<プラグイン名>:<ローカル FormID>` の行を1行書く。出荷時の設定ファイルには 2 行がコメントアウトされた状態で入っています。行頭の ; を外すと有効になります。石の容量・中身の魂・対になる充填済みフォームはプラグインのレコードから読み取るため、ここに書くのは識別子だけ。
- 壊れた行があってもその行だけが無効になり、機能全体は止まらない。設定した行ごとに状態をログへ記録する。例: `RESOLVED`、`PLUGIN_ABSENT`(プラグインがロード順に無い)、`FORM_NOT_FOUND`(プラグインはあるがフォームが無い)、`MALFORMED`(行の書式が誤っている)。
- ESP とセーブレコードへの変更はなく、新しいセーブデータも増えない。「魂石を使用」は追加した魂石にも適用され。`[ExtraGems]` の 2 行が出荷時のままコメントアウトされた状態の場合、セクションが無い場合、または空の場合、v1.1.0 の動作は v1.0.0 と完全に同じ。

## v1.0.0

- Released as the stable v1.0.0 — the mod's first public release. It automatically recharges melee weapons and bows/crossbows; the left-hand weapon and staff options are OFF by default and can be turned on in the MCM. No changes to the ESP or to any save record.
- Equip-time evaluation and a recharge sound are planned for a later update.
- The comments in the settings file were rewritten in plain language for players. Every setting key and every default value is unchanged.
- Future updates will keep settings and saves compatible; upgrading will only replace the files.

- 安定版 v1.0.0 として初公開した。近接武器・弓/クロスボウの自動充填に対応し、左手武器・杖の自動充填はオプション(既定OFF、MCMでON)。ESP とセーブレコードへの変更はない。
- 装備時評価と充填音は、今後のアップデートで対応する。
- 設定ファイルのコメントを、利用者向けの平易な説明文に書き直した。設定キーと既定値はすべて変更していない。
- 今後のアップデートでも設定・セーブとの互換性を保つ。更新はファイルの置き換えだけで済む。

## v0.4.0

- Added an opt-in staff (`Staves`) auto-recharge, OFF by default; enable it in the MCM.
- A staff is evaluated after its cast finishes, so recharging never interrupts an in-progress cast.
- Three staff cast types are supported: instant, continuous beam, and summon.
- Coexists with an external Staff Recharge perk (Vokrii/Ordinator): the perk's own charge restoration is never mixed into this mod's gain, and a defect that could stop the recharge partway when both were active together is fixed.
- Constant-effect staves are out of scope and are never evaluated.
- No changes to existing settings or saves; v0.3.0 settings and saves remain compatible in both directions.

- 杖(`Staves`)の自動充填を追加した。既定はOFFで、MCMでONにできる。
- 杖は詠唱が終わった後に評価するため、進行中の詠唱を中断しない。
- 瞬間発動・連続放射・召喚の3種類の杖に対応する。
- 外部のStaff Recharge perk(Vokrii/Ordinator)と共存する。perkによる回復分は当方の充填量に混ぜず、両方を併用すると途中で止まることがあった不具合を修正した。
- 常時効果の杖は対象外で、評価しない。
- 既存の設定・セーブへの変更はなく、v0.3.0の設定・セーブと双方向に互換性を保つ。

## v0.3.0

- Added an opt-in "Recharge left-hand weapons" (`DualWield`) setting, OFF by default. When ON, the right-hand weapon is evaluated first; once it is fully charged (or found not eligible), the same hit independently evaluates the left-hand weapon.
- The left and right weapon instances are tracked separately, even when both hands carry the same weapon; a soul or star spent recharging the right-hand weapon is never reused for the left hand.
- Added a final time-budget check: if recharge processing takes too long, it stops before any soul is consumed rather than after.
- Diagnostics now shows which hand was last evaluated and the last recorded charge value for each hand.
- No new save records; the only addition is the `DualWield` setting key. With `DualWield` OFF, behavior and save compatibility remain identical to v0.2.2.

- 「左手の武器も充填」設定(`DualWield`)を追加した。既定はOFF。ONにすると、まず右手の武器を評価し、右手が満充填(または対象外と判定)になった後、同じ命中で左手の武器も独立に評価する。
- 左右の武器個体を別々に識別する。両手が同じ武器を装備していても混同せず、右手の充填に使った魂石・星を左手で再利用することはない。
- 充填処理に時間がかかりすぎる場合の最終チェックを追加した。基準を超えた場合は魂石を消費する前に処理を停止する。
- 診断ページに、最後に評価した対象の手と、左右それぞれの最後の充填量を表示するようになった。
- 新規セーブレコードの追加はなく、追加は`DualWield`設定キーのみ。`DualWield`がOFFなら、動作とセーブ互換性はv0.2.2と同一。

## v0.2.2

- The MCM now opens on the Settings page by default; select Diagnostics from the page list to view it. (Previously Diagnostics opened first.)
- Diagnostics rows now show plain-language text with the underlying code in parentheses, for example "Recharged (VERIFIED)", instead of a raw code.
- Three diagnostics labels were renamed for clarity: "last recharge result", "recharge in progress", and "last stop reason".
- No changes to the DLL, ESP, or soul gem consumption logic. Existing saves and settings remain compatible.

- MCMを開くと既定で「設定」ページが表示されるようになった(以前は「診断」ページが先に表示されていた)。「診断」はページの一覧から選ぶと表示できる。
- 診断ページの各項目は、これまでの内部コードの表示に代えて、平易な説明文の末尾に(コード)を添えた表示になった。例:「充填しました (VERIFIED)」。
- 診断の項目名を3つ、分かりやすく変更した。「最後の自動充填の結果」「いま処理中の充填」「最後に停止した理由」。
- DLL、ESP、魂石の消費処理の変更はない。既存のセーブと設定ファイルはそのまま使える。

## v0.2.1

- If bow/crossbow release detection stops working partway through a session, the mod now handles this automatically: after 2 consecutive eligible hits that cannot be linked to a detected release, it falls back to the v0.1 hit-based evaluation. The hit that triggers the fallback is still recharged. Evaluation returns to release-based (post-fire) mode as soon as a release is detected again.
- Fixed the MCM's native-version display, which previously showed 0.1.0; it now shows 0.2.1.
- Diagnostics adds two new INI settings under `[Ranged]`: `UnlinkedHitLimit` (default 2) and `UnlinkedHitWindowMs` (default 3000), which control how many unlinked hits and how much time trigger the fallback above.
- Two further diagnostic-only INI settings, `DiagDropReleaseAfter` and `DiagDropReleaseCount` (default 0 and 2), let a tester temporarily and deliberately hide a bounded number of release detections to exercise the fallback and recovery behavior. The shipped default (`DiagDropReleaseAfter=0`) disables this and has no effect on normal play.
- No consumption logic, ESP, `Native.pex`, or MCM PEX changes; the MCM settings pages are unchanged.

- 弓・クロスボウの発射(リリース)検出がセッション中に失われた場合、以後は自動で対応する。適格な命中が検出済みの発射に結び付かない状態が2回連続すると、命中時のみで判定するv0.1相当の動作に切り替わる。切り替わりを判定したその命中も、通常どおり再充填の対象になる。発射検出が再び確認できた時点で、発射後判定の動作に戻る。
- MCMの診断ページのnative版表示を修正した。これまで0.1.0と表示していたが、0.2.1と表示するようになった。
- 診断用の設定として、INIの`[Ranged]`に`UnlinkedHitLimit`(既定2)と`UnlinkedHitWindowMs`(既定3000)を追加した。上記の切り替えに使う連続回数と時間の設定。
- さらに診断専用の設定として`DiagDropReleaseAfter`と`DiagDropReleaseCount`(既定0と2)を追加した。検証者が発射検出を一時的に一定数だけ意図的に隠し、切り替え・復帰の動作を確認するための設定であり、出荷時の既定(`DiagDropReleaseAfter=0`)では無効で通常プレイに影響しない。
- 消費処理、ESP、`Native.pex`、MCM PEXの変更はない。MCMの設定ページも変更しない。

## v0.2.0

- With Recharge bows and crossbows enabled, v0.2.0 adds evaluation after firing an enchanted bow or crossbow, including missed shots. Charge must be at or below the configured threshold; filled soul gems or stars are still required.
- Turning Recharge bows and crossbows (RangedWeapons) OFF disables automatic bow and crossbow evaluation, both after firing and on hit. This matches v0.1 with the same setting OFF; right-hand melee behavior is unchanged. OFF does not select hit-only bow recharging.
- If your animation setup cannot provide firing detection, Diagnostics shows that detection is unavailable or that only hits are supported. In this fallback, eligible hits use the v0.1 behavior and missed shots cannot recharge. Before detection is confirmed, the status is unverified; successful detection shows available.
- The MCM still has nine settings. RangedWeapons is an existing setting, remains ON by default, and keeps its saved value when updating.
- Soul gem selection, star reuse, protection settings, charge gained, Enchanting experience, the Soul Gems Used statistic, manual hotkeys, and settings storage are unchanged. There is no free recharge.
- Dual wielding, left-hand weapons, staves, equip-time evaluation, followers, and NPCs remain unsupported. The v0.1 descriptions below are historical; this section describes the v0.2.0 changes.

- 弓とクロスボウの充填を有効にすると、v0.2.0では付呪された弓とクロスボウの発射後にも充填を判定する。外れた矢も対象になる。残量が設定したしきい値以下で、魂の入った魂石または星を所持している必要がある。
- 「弓とクロスボウを充填」(RangedWeapons)をOFFにすると、発射後と命中時の両方で弓とクロスボウの自動充填を停止する。同じ設定をOFFにしたv0.1相当となり、右手系近接武器の動作は変わらない。OFFは弓の命中時のみの充填を選ぶ設定ではない。
- 使用中のアニメーション構成で発射を検出できない場合、診断ページに検出を利用できない状態、または命中時のみの対応を表示する。この場合は適格な命中時にv0.1と同じ動作となり、外れた矢では充填しない。検出を確認する前は未確認、確認できると利用可能と表示する。
- MCMの設定は引き続き9項目となる。RangedWeaponsは既存の設定で、既定値はONのままとし、更新時には保存済みの値を引き継ぐ。
- 魂石の選択、星の再利用、保護設定、回復量、付呪スキル経験値、Soul Gems Used統計、手動キー、設定の保存方法は変更しない。無料の充填は行わない。
- 二刀流、左手武器、杖、装備時の判定、従者、NPCは引き続き対象外となる。以下のv0.1の説明は旧版の記録とし、v0.2.0の変更は本節に記載する。

## 0.1.0 — first public draft / 初回公開原稿

- Added automatic recharge for one equipped right-hand-class melee weapon after an eligible hit.
- Added bow and crossbow hit-time evaluation, enabled by default.
- Added star-first selection, smallest-contained-soul ordering, black-soul-gem preservation, and no-free-recovery economy.
- Added the two-page MCM with nine settings, diagnostics, profile `user.json` settings, and a DLL-missing stopped stub path.
- Documented the default-enabled (`Enabled=1`) automatic path, F11 one-shot recharge, F10 log-only diagnostic snapshot, and `USER_CORRUPT` fail-safe.
- Added EN/JA installation, update, removal, rollback, compatibility, and permissions documentation.

- 適格な命中後、装備中の右手系近接武器 1 本を自動再充填する機能を追加。
- 弓とクロスボウの命中時評価を追加。既定で有効。
- 星優先、魂の小さい順、黒魂石温存、無料回復なしの経済を追加。
- 9 設定の 2 ページ MCM、診断、profile の `user.json` 設定、DLL 不在時の停止用 stub 経路を追加。
- 既定有効（`Enabled=1`）の自動経路、F11 の 1 回充填、F10 のログ専用診断スナップショット、`USER_CORRUPT` 自動停止を記載。
- 導入、更新、削除、rollback、共存、許諾の EN/JA 説明を追加。
