"""Package the Iron Lungs projectile-impact candidate as combined 2.7.5-beta1.

python iron_lungs_impact.py BASELINE.zip --verify-plugin-only
python iron_lungs_impact.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip --source-commit SHA
The complete ESP is copied byte-for-byte. Native build provenance is mandatory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
from esp import records

VERSION = '2.7.5-beta1'
NATIVE_VERSION = '1.4.5'
BASE_SHA = 'ac19ebf846c1cbd68bd832dac19fb3fbebcf6adb70aefee97b130bdfa171af92'
PLUGIN = 'Biggie Traits - Combined.esp'
DLL = 'SKSE/Plugins/BiggieTraitMechanics.dll'
SOURCE_PREFIX = 'Documentation/ThreeTraits/NativeSource/'
BUILD_PREFIX = 'Documentation/ThreeTraits/NativeBuild/'
DOCS = 'Documentation/IronLungs/ImpactDelivery/'
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[2] / 'ThreeTraits/NativeSource'

README = '''BIGGIE TRAITS COMBINED v2.7.5-beta1 — IRON LUNGS IMPACT DELIVERY CANDIDATE

Iron Lungs now associates a manual Unrelenting Force hit with the exact
projectile launched by that cast at the projectile-impact boundary. Matching
uses the original spell, target and caster identities around the accepted
magic-target application. A successful matching target receives the existing
magic-damage bonus at most once per cast. This fixes the missing association
observed in the submitted diagnostic log; successful in-game damage delivery
still needs to be confirmed.

The incoming spell-magnitude modifier call also had an extra argument in the
previously unreachable damage routine. Its parameter list is corrected while
preserving the intended outgoing and incoming perk calculations.

WHAT THE SUBMITTED LOG SHOWED
The submitted log reported native version 1.4.3. It recorded 20 manual
Unrelenting Force casts with one projectile each and correctly calculated
25%-of-current-Stamina bonuses, for example 420 Stamina produced 105 bonus.
However, every FIND observation used reference 00000014 and reported cast=0;
there were no Iron Lungs HIT records. The observed failure was between the
recorded projectile launch and the hit-to-cast association. The log establishes
that the bonus was calculated, not that it reached a target. The raw session
log is not included in the package.

PRESERVED BALANCE AND EFFECT RULES
- Bonus remains non-elemental magic equal to 25% of Stamina before payment.
- Existing shout perk scaling, magic resistance and spell absorption remain
  in the engine's effect path. No direct Health subtraction or armor-based
  replacement damage is added.
- One cost per manual release and one bonus per target per cast remain.
- Cost remains 25% maximum Stamina adjusted by shout recovery, with a minimum
  of 10% maximum Stamina and the existing strict Stamina gate.
- Unrelenting Force adds no new shout cooldown and preserves recovery left
  by another shout. All three words remain granted and unlocked.
- Any manual shout still causes 20% more physical damage taken for 3 seconds,
  refreshing without stacking. Skald casts remain exempt from this drawback
  and from Iron Lungs' Stamina cost and bonus.
- Skald's v2.7.4 self-cast timing, cooldowns, diagnostics and pending-request
  rules are preserved. Its first-word spells are unchanged.
- Existing artwork, scripts, INIs, trait records and other traits are preserved.

INSTALL AND CHECK
1. Exit Skyrim and replace the previous combined package in MO2.
2. Keep Biggie Traits - Combined.esp at its existing load-order position.
3. Ensure BiggieTraitMechanics.dll wins conflicts and its log reports 1.4.5.
   The submitted failure log came from 1.4.3, so verify the loaded version.
4. Load an existing save. Removing and reselecting the trait is not required.
5. Manually shout Unrelenting Force at a living target with sufficient Stamina.
   Retain the CAST, impact/association and HIT diagnostic lines and observe
   target Health. Compare matched targets, resistances, perk state and word
   level; do not infer the delivered bonus from the CAST calculation alone.
6. Check more than one target in a cast and repeat with each word level.
   Each valid target should receive the bonus once; repeated processing must
   not apply it again or charge another Stamina cost.
7. Verify insufficient-Stamina blocking, existing shout recovery, the
   three-second vulnerability and Skald's automatic Unrelenting Force exemption.
8. Recheck Skald's Kyne's Peace and other stored shouts after replacing the
   package. Keep BiggieTraitMechanics.log if any behavior remains incorrect.

VALIDATION / BETA
Packaging requires the Windows build and seven native rule-test suites to
pass, with source and DLL hashes matching the supplied build commit. The new
suite covers impact association rules. Tests do not establish actual Skyrim
projectile behavior or confirm delivered in-game damage.
Exact final perk scaling also needs a controlled in-game check; an accepted
effect and its requested magnitude do not establish final Health loss.
The full ESP,
including all 66 gameplay records and the TES4 header, is byte-identical to
v2.7.4-beta1. There is still exactly one ESP, with no new masters or records.
The revised delivery has not yet been verified in game.

PREVIOUS RELEASE NOTES (historical)

'''

ANALYSIS = '''# Iron Lungs projectile-impact association — 2.7.5-beta1

The submitted diagnostic log identified its loaded native plugin as **1.4.3**.
It recorded **20 manual Unrelenting Force casts**, each with one launched
projectile, and calculated the intended 25%-of-pre-payment-Stamina bonus.
For example, 420 Stamina yielded a 105-point bonus calculation.

Every recorded FIND lookup used reference **00000014** and returned **cast=0**;
there were **zero HIT records**. This isolates an observed missing association
between the tracked cast projectile and subsequent hit processing. It does not
demonstrate that damage was delivered merely because CAST reported a bonus.
The user's raw log is deliberately excluded from the release.

## Candidate correction

The native adapter now observes the actual projectile-impact boundary and
looks up the exact launched projectile handle. Matching is scoped to the
original spell, caster and target, and the normal accepted magic-target
application precedes the existing once-per-target bonus handling.

The incoming spell-magnitude perk call now receives its correct argument list,
without the extra caster argument. Outgoing and incoming magnitude entry points
have different signatures. This is a forwarding correction, not a balance change.

The original non-elemental damage effect, shout-perk scaling, magic resistance
and absorption handling remain in place. The update does not force Health
loss, invent a separate physical hit, change the bonus magnitude, add another
cost or let Skald automatic casts receive Iron Lungs benefits. Spell records,
the entire ESP and every existing script, INI and texture remain unchanged.
Skald's deferred self-cast implementation from 2.7.4 is preserved.

## Verification still required

The native Windows build and seven rule suites check compilation and the
isolated rules. They cannot establish that Skyrim invokes the relevant
projectile path as expected for the user's winning shout records. In-game
testing must confirm that a tracked manual cast reaches matching targets,
produces the bonus once per target, and still obeys resistance, absorption,
Stamina and Skald-exemption rules.

Power Affects Magnitude being off does not by itself establish that subsequent
perk adjustments are skipped. The explicit magnitude override is preserved,
but exact final scaling remains unverified. Accepted APPLY records confirm
delivery/acceptance, not the final active-effect magnitude or target Health loss.

First verify that the log reports **1.4.5**. Retain CAST, impact/association
and HIT records while checking the target's Health. Compare equivalent targets,
word levels, resistances and perk state. Multiple targets and multiple effects
must not create duplicate bonuses. Existing Skald diagnostics and historical
release analyses remain archived under Documentation/Skald.
'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encode(value):
    return (json.dumps(value, indent=2) + '\n').encode()


def read_zip(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        names = [n for n in archive.namelist() if not n.endswith('/')]
        normalized = [n.replace('\\', '/') for n in names]
        assert len(normalized) == len(set(normalized)), 'Duplicate ZIP members'
        return {n.replace('\\', '/'): archive.read(n) for n in names}


def verify_plugin(original, packaged):
    assert packaged == original, 'The entire ESP must remain byte-identical'
    items = list(records(packaged))
    assert items[0][0][:4] == b'TES4'
    assert len(items) - 1 == 66
    assert struct.unpack_from('<I', items[0][0], 8)[0] & 0x200, 'ESL flag missing'
    return {
        'entire_esp_byte_identical': True, 'esp_sha256': sha(packaged),
        'gameplay_records_byte_identical': 66, 'changed_record_fields': {},
        'tes4_header_byte_identical': True, 'formids_unchanged': True,
        'new_records': 0, 'new_masters': 0, 'esl_flag_preserved': True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('native_build', type=Path, nargs='?')
    parser.add_argument('output', type=Path, nargs='?')
    parser.add_argument('--source-commit', help='Required exact native build commit SHA')
    parser.add_argument('--verify-plugin-only', action='store_true')
    args = parser.parse_args()
    assert sha(args.baseline.read_bytes()) == BASE_SHA, 'Wrong baseline package'
    old = read_zip(args.baseline)
    previous = json.loads(old['Validation.json'])
    for name, digest in previous['files'].items():
        assert sha(old[name]) == digest, name
    files = dict(old)
    report = verify_plugin(old[PLUGIN], files[PLUGIN])
    if args.verify_plugin_only:
        print(json.dumps(report, indent=2))
        return
    if not args.native_build or not args.output or not re.fullmatch(r'[0-9a-f]{40}', args.source_commit or ''):
        parser.error('Packaging requires NATIVE_BUILD.zip OUTPUT.zip and --source-commit SHA')
    native = read_zip(args.native_build)
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == NATIVE_VERSION and info['combined_version'] == VERSION
    assert info['source_commit'] == args.source_commit
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    assert info['in_game_tested'] is False
    assert native[DLL][:2] == b'MZ' and sha(native[DLL]) == info['dll_sha256']
    for name in ('src/SkaldDiagnostics.h', 'src/SkaldRuntime.h', 'src/SkaldDeferred.h',
                 'tests/rules_tests.cpp', 'tests/lab_tests.cpp', 'tests/skald_tests.cpp',
                 'tests/iron_lungs_tests.cpp', 'tests/overexertion_tests.cpp',
                 'tests/skald_deferred_tests.cpp', 'src/IronLungsGrant.h', 'src/Overexertion.h',
                 'src/IronLungsImpact.h', 'tests/iron_lungs_impact_tests.cpp'):
        assert name in info['source_sha256_lf'], name
    for name, digest in info['source_sha256_lf'].items():
        assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
    changed_native = {
        'src/main.cpp', 'src/IronLungsRuntime.h', 'CMakeLists.txt',
        'tools/build_windows.ps1', 'README.md',
    }
    new_native = {'src/IronLungsImpact.h', 'tests/iron_lungs_impact_tests.cpp'}
    expected_main = old[SOURCE_PREFIX + 'src/main.cpp'].replace(b'1.4.4', b'1.4.5').replace(b'{1,4,4,0}', b'{1,4,5,0}')
    assert (NATIVE / 'src/main.cpp').read_bytes() == expected_main, 'main.cpp must change only version metadata'
    for name, data in old.items():
        if name.startswith(SOURCE_PREFIX):
            relative = name[len(SOURCE_PREFIX):]
            if relative not in changed_native:
                assert (NATIVE / relative).read_bytes() == data, relative
    for name in new_native:
        assert SOURCE_PREFIX + name not in old, name
    files[DLL] = native[DLL]
    for name, data in native.items():
        if not name.startswith('SKSE/'):
            files[BUILD_PREFIX + name] = data
    source_names = {name[len(SOURCE_PREFIX):] for name in old if name.startswith(SOURCE_PREFIX)} | new_native
    for name in sorted(source_names):
        files[SOURCE_PREFIX + name] = (NATIVE / name).read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files[DOCS + 'README.md'] = ANALYSIS.encode()
    files[DOCS + 'v2_7_4-Baseline-Validation.json'] = old['Validation.json']
    for name in ('iron_lungs_impact.py', 'esp.py'):
        files[DOCS + 'Rebuild/' + name] = (ROOT / name).read_bytes()
    files[DOCS + 'Rebuild/README.txt'] = (
        'REBUILD v2.7.5-beta1\n\n'
        'Run from this folder with Python 3 and keep the complete extracted\n'
        'package layout. The script finds the archived native sources.\n'
        'Obtain the unchanged v2.7.4-beta1 baseline ZIP with SHA-256:\n'
        + BASE_SHA + '\n\n'
        'The Windows-built native ZIP must contain BuildInfo.json and the DLL\n'
        'matching all archived native source hashes and this source commit:\n'
        + args.source_commit + '\n\n'
        'python iron_lungs_impact.py BASELINE.zip --verify-plugin-only\n'
        'python iron_lungs_impact.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip '
        '--source-commit ' + args.source_commit + '\n\n'
        'The output must not already exist. Native build instructions are in\n'
        'Documentation/ThreeTraits/NativeSource/README.md. No ESP edits occur.\n'
    ).encode()
    allowed = {DLL, 'README.txt', 'Validation.json'}
    for name, data in old.items():
        if name not in allowed and not name.startswith((BUILD_PREFIX, SOURCE_PREFIX)):
            assert files[name] == data, name
    assert all(files[n] == data for n, data in old.items()
               if n.lower().endswith(('.esp', '.esm', '.esl', '.pex', '.psc', '.ini', '.dds')))
    report.update(
        version=VERSION, native_build=info, in_game_tested=False,
        change='Associate manual Unrelenting Force with its launched projectile at impact and gate bonus delivery on the matching accepted target',
        submitted_log_native_version='1.4.3', submitted_log_manual_casts=20,
        submitted_log_projectiles_per_cast=1, submitted_log_hit_records=0,
        observed_failure='FIND used reference 00000014 and returned cast=0 despite tracked launches',
        actual_projectile_handle_required=True, scoped_spell_caster_target_identity=True,
        accepted_target_application_required=True, bonus_once_per_target_per_cast=True,
        bonus_formula='25% of current Stamina before payment; non-elemental magic',
        cost_gate_and_recovery_rules_preserved=True,
        native_engine_perk_resistance_absorption_path_preserved=True,
        incoming_magnitude_entrypoint_argument_list_corrected=True,
        final_native_magnitude_scaling_verified_in_game=False,
        skald_automatic_casts_exempt=True, skald_self_cast_timing_preserved=True,
        skald_sources_byte_identical_to_v2_7_4=True,
        all_iron_lungs_balance_rules_preserved=True,
        full_word_grant_preserved=True, other_trait_rules_preserved=True,
        papyrus_scripts_unchanged=True, all_thumbnails_preserved_from_v2_7_4=True,
        native_source_changes=sorted(changed_native), new_native_sources=sorted(new_native),
        existing_native_rule_suites=6, total_native_rule_suites=7,
    )
    files[DOCS + 'Validation.json'] = encode(report)
    manifest = {
        'version': VERSION, 'baseline': args.baseline.name, 'baseline_sha256': BASE_SHA,
        'traits': previous['traits'], 'validation': report, 'trait_mechanics_dll': info,
        'artwork_revision': previous['artwork_revision'], 'artwork_validation': previous['artwork_validation'],
    }
    manifest['files'] = {n: sha(data) for n, data in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = encode(manifest)
    if args.output.exists():
        raise FileExistsError(args.output)
    with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(args.output) as archive:
        assert archive.testzip() is None
        assert sum(n.lower().endswith(('.esp', '.esm', '.esl')) for n in archive.namelist()) == 1
        verify_plugin(old[PLUGIN], archive.read(PLUGIN))
        for name, digest in manifest['files'].items():
            assert sha(archive.read(name)) == digest, name
    print(json.dumps({
        'output': str(args.output.resolve()), 'sha256': sha(args.output.read_bytes()),
        'bytes': args.output.stat().st_size, 'entire_esp_byte_identical': True,
        'gameplay_records_preserved': report['gameplay_records_byte_identical'],
        'native_commit': info['source_commit'], 'in_game_tested': False,
    }, indent=2))


if __name__ == '__main__':
    main()
