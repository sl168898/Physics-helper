"""Package the Skald deferred self-cast candidate as combined 2.7.4-beta1.

python skald_timing.py BASELINE.zip --verify-plugin-only
python skald_timing.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip --source-commit SHA
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

VERSION = '2.7.4-beta1'
NATIVE_VERSION = '1.4.4'
BASE_SHA = 'd40219f095a9d5339b8b9f742abe82ec026d365d71ad1f33a46c957a66ef8b5e'
PLUGIN = 'Biggie Traits - Combined.esp'
DLL = 'SKSE/Plugins/BiggieTraitMechanics.dll'
SOURCE_PREFIX = 'Documentation/ThreeTraits/NativeSource/'
BUILD_PREFIX = 'Documentation/ThreeTraits/NativeBuild/'
DOCS = 'Documentation/Skald/SelfCastTiming/'
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[2] / 'ThreeTraits/NativeSource'

README = '''BIGGIE TRAITS COMBINED v2.7.4-beta1 — SKALD SELF-CAST TIMING CANDIDATE

Skald's stored self-delivery shout is now queued at the melee power attack
and released after the original player update. The intended result is for
the engine to process the triggering attack before the self-buff is applied,
so an instant restoration is less likely to be wasted against a full resource
bar before the power attack spends Stamina. This timing change still requires
an in-game check; the diagnostic snapshots alone do not establish the exact
order of Skyrim's actor-value changes.

Other delivery types still cast immediately. The exact loaded first-word
spell, its conditions, native magnitude, duration and winning overrides are
retained. There is no extra healing, Stamina refund, replacement spell,
magnitude correction, or direct actor-value modification. Skald's cooldown
and Echoing Steel are reserved once at the triggering attack, as before.
The 10/6/3-second recovery at base Speech 0/50/100 is unchanged.

The pending self cast stores only form IDs and is not serialized into saves.
It is canceled on loading, death, trait loss, stored-choice changes or a
killmove. Pausing holds it until gameplay resumes. Native log lines record
QUEUED and RELEASE resource snapshots, followed by the existing before,
immediate and delayed self-cast diagnostics. These observations are read-only.

WHAT THE SUBMITTED KYNE'S PEACE LOG SHOWED
All six casts created the Health, Stamina and Magicka restoration effects at
magnitude 30.25, although the loaded first-word spell supplied magnitude 100.
The source of this scaling is unknown; this update deliberately preserves it.
One full-Stamina cast went from 120 to 50.474403, a net expenditure of
69.525597. A later cast with room for healing went from 104.264435 to
64.987730. Adding 30.25 restoration and subtracting the earlier expenditure
predicts 64.988838, within 0.001108 of the observation. This is strong evidence
that restoration was occurring but could be hidden by the resource cap and
power-attack expenditure. It is not a direct measurement of the attack cost
or proof of the engine event order; regeneration and other effects can affect
the observations. A compact numerical analysis is archived with this release;
the user's raw session log is not included.

BUFF-SHOUT SCOPE
The timing change applies generically to loaded first-word spells using Self
delivery, including the inspected Kyne's Peace, Predator's Might, Elemental
Fury, Become Ethereal, Slow Time and Aura Whisper records. Dragon Aspect and
Battle Fury use the same path if their winning first-word spells use Self
delivery, but their exact records were unavailable locally. Compatibility
with all buff shouts has not been verified in game. The earlier static record
audit is retained under Documentation/Skald/BuffCastAudit.

PRESERVED IRON LUNGS
All three Unrelenting Force words remain granted and unlocked. Manual UF
retains its Stamina gate and cost, magic-damage bonus and no new cooldown.
The drawback remains 20% more physical damage for 3 seconds after any manual
shout, refreshing without stacking. Skald casts remain exempt. The existing
trait artwork and the other trait rules are unchanged.

INSTALL AND CHECK
1. Exit Skyrim and replace the previous combined package in MO2.
2. Keep Biggie Traits - Combined.esp enabled at its existing load-order position.
3. Ensure BiggieTraitMechanics.dll wins conflicts and its log reports 1.4.4.
4. Load an existing save; removing and reselecting the trait is not required.
5. Store Kyne's Peace, leave substantial Stamina missing, and power attack
   when Skald is ready. Compare it with a manual first-word cast under the
   same effects. Retain the QUEUED, RELEASE and subsequent diagnostic lines.
6. Check Predator's Might, Elemental Fury and Dragon Aspect independently;
   compare matching first-word casts, not a manual three-word version.
7. Verify offensive stored shouts, recovery, Echoing Steel and Iron Lungs.

VALIDATION / BETA
Packaging requires the Windows build and six native rule-test suites to pass,
with source and DLL hashes matching the supplied build commit. The additional
suite covers pending self-cast state; unit tests do not establish Skyrim's
actual attack-cost ordering or self-buff behavior. The entire ESP, all 66
gameplay records and its TES4 header are byte-identical to v2.7.3-beta1.
All existing scripts, INIs and artwork are preserved. There is exactly one
ESP. The revised timing has not yet been validated in game.

PREVIOUS RELEASE NOTES (historical)

'''

ANALYSIS = '''# Skald self-cast timing — 2.7.4-beta1

The 1.4.3 diagnostic log contains six Kyne's Peace casts. All instantiated
the three restore effects at magnitude **30.25**. The source spell supplied
100 for each effect. No specific cause for the reduction has been established;
the timing update does not change magnitude or grant additional restoration.

| Cast | Stamina before / maximum | Stamina after approximately 0.252 seconds | Inferred net expenditure with capped 30.25 restoration |
| --- | ---: | ---: | ---: |
| 1 | 102.742990 / 120 | 21.366768 | 98.633232 |
| 2 | 100.417960 / 120 | 21.367730 | 98.632270 |
| 3 | 120 / 120 | 21.366623 | 98.633377 |
| 4 | 120 / 120 | 50.474403 | 69.525597 |
| 5 | 261.446300 / 270 | 200.466610 | 69.533390 |
| 6 | 104.264435 / 270 | 64.987730 | 69.526705 |

The model is `after = min(maximum, before + 30.25) - net expenditure`.
The last column is derived, not a separately measured attack cost. Other
resource changes during the observation gap contribute to net expenditure.
Using cast 4's inferred expenditure, cast 6 is predicted at 64.988838 versus
64.987730 observed. The agreement supports healing already taking effect,
with much of its benefit sometimes lost at the resource cap before expenditure.
The log did not capture each actor-value modification, so it cannot prove the
precise engine event order. Health and Magicka were full in every snapshot;
their unchanged values cannot establish failure. No other buff shout appeared
in that session's diagnostic cast records.

## Candidate change

Only self-delivery casts are deferred until the post-original player update.
Other delivery types remain immediate. The existing first-word spell and
normal casting mode are retained; there is no fallback healing, cost refund,
synthetic shout event, magnitude override or spell-record modification.
Cooldown and Echoing Steel are reserved once at the original attack trigger.
Pending requests contain form IDs only, are not saved, and are canceled by
load, death, trait loss, choice changes and killmoves. Pause holds a request.

QUEUED and RELEASE diagnostics expose the resources at the scheduling points,
followed by the prior effect and resource snapshots. A missing-resource test
and comparison with a normal manual first-word shout remain necessary.
The six rule suites and Windows compilation do not substitute for that test.
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
                 'tests/skald_deferred_tests.cpp', 'src/IronLungsGrant.h', 'src/Overexertion.h'):
        assert name in info['source_sha256_lf'], name
    for name, digest in info['source_sha256_lf'].items():
        assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
    changed_native = {
        'src/main.cpp', 'src/SkaldRuntime.h', 'CMakeLists.txt',
        'tools/build_windows.ps1', 'README.md',
    }
    new_native = {'src/SkaldDeferred.h', 'tests/skald_deferred_tests.cpp'}
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
    for path in NATIVE.rglob('*'):
        if path.is_file() and '_build' not in path.parts:
            files[SOURCE_PREFIX + path.relative_to(NATIVE).as_posix()] = path.read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files[DOCS + 'README.md'] = ANALYSIS.encode()
    files[DOCS + 'v2_7_3-Baseline-Validation.json'] = old['Validation.json']
    for name in ('skald_timing.py', 'esp.py'):
        files[DOCS + 'Rebuild/' + name] = (ROOT / name).read_bytes()
    files[DOCS + 'Rebuild/README.txt'] = (
        'REBUILD v2.7.4-beta1\n\n'
        'Run from this folder with Python 3 and keep the complete extracted\n'
        'package layout. The script finds the archived native sources.\n'
        'Obtain the unchanged v2.7.3-beta1 baseline ZIP with SHA-256:\n'
        + BASE_SHA + '\n\n'
        'The Windows-built native ZIP must contain BuildInfo.json and the DLL\n'
        'matching all archived native source hashes and this source commit:\n'
        + args.source_commit + '\n\n'
        'python skald_timing.py BASELINE.zip --verify-plugin-only\n'
        'python skald_timing.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip '
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
        change='Queue self-delivery Skald cast until post-original player update; other delivery types remain immediate',
        confirmed_engine_event_order=False, all_buff_shouts_in_game_verified=False,
        exact_loaded_first_word_preserved=True, native_magnitude_override=0,
        no_extra_healing_or_stamina_refund=True, source_of_30_25_magnitude_unknown=True,
        skald_recovery_seconds_by_base_speech={'0': 10, '50': 6, '100': 3},
        cooldown_and_echo_reserved_once_at_attack=True,
        pending_request='form IDs only; unsaved; canceled on load, death, trait loss, choice change or killmove; pause holds',
        diagnostics='QUEUED/RELEASE resources plus bounded read-only self-cast effect/AV snapshots',
        exact_records_unavailable=['Dragon Aspect', 'Battle Fury'],
        all_iron_lungs_benefits_costs_and_drawback_preserved=True,
        full_word_grant_preserved=True, other_trait_rules_preserved=True,
        papyrus_scripts_unchanged=True, all_thumbnails_preserved_from_v2_7_3=True,
        native_source_changes=sorted(changed_native), new_native_sources=sorted(new_native),
        existing_native_rule_suites=5, total_native_rule_suites=6,
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
