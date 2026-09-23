"""Package the Skald self-buff casting candidate as combined 2.7.3-beta1.

python skald_buffs.py BASELINE.zip --verify-plugin-only
python skald_buffs.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip --source-commit SHA
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

VERSION = '2.7.3-beta1'
NATIVE_VERSION = '1.4.3'
BASE_SHA = 'db88005f800bd4cdfdc2fcfddb636ee5623e7764f3516424ba4ada687a814e98'
REQUIEM_SHA = 'f736c19a3a0ebfbafdaa997d22bc3b1e94e8730bb69d70ede24918b5dd68f97b'
PLUGIN = 'Biggie Traits - Combined.esp'
DLL = 'SKSE/Plugins/BiggieTraitMechanics.dll'
SOURCE_PREFIX = 'Documentation/ThreeTraits/NativeSource/'
BUILD_PREFIX = 'Documentation/ThreeTraits/NativeBuild/'
DOCS = 'Documentation/Skald/BuffCastAudit/'
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[2] / 'ThreeTraits/NativeSource'
AUDIT = ROOT / 'skald-buff-audit.json'
if not AUDIT.exists():
    AUDIT = ROOT.parent / 'Requiem-self-buff-records.json'

README = '''BIGGIE TRAITS COMBINED v2.7.3-beta1 — SKALD SELF-BUFF CASTING CANDIDATE

Skald now requests a normal immediate cast of the stored first-word spell.
The second CastSpellImmediate argument changes from true to false, matching
the normal casting path used by other native spell-casting implementations.
The prior value was a candidate cause of self-buff spells doing nothing.
This is a candidate fix, not a confirmed engine-level diagnosis; an in-game
check is still needed for Kyne's Peace and the other buff shouts.

The exact first-word spell and effects from the loaded shout are retained,
including winning load-order overrides. No replacement buff spells, extra
healing, synthetic shout events, or direct actor-value changes are added.
The fix is generic and is not restricted to Kyne's Peace. Self-delivery spells
still target the player. Skald still releases only the first word and keeps
its own 10/6/3-second recovery at base Speech 0/50/100.

READ-ONLY DIAGNOSTICS
Bounded Skald diagnostic lines in BiggieTraitMechanics.log record the loaded
spell/effects and player Health, Stamina, Magicka and matching active effects
before a self cast, immediately afterward, and after 0.25 seconds of gameplay.
These observations do not restore actor values or reapply missing effects.
Delayed observations can include ordinary regeneration and other mod activity;
their differences are not presented as a measured amount restored by Skald.

FIRST-WORD RECORD AUDIT
The supplied Requiem.esp contains these baseline values before perk scaling:
- Kyne's Peace: restore 100 Health, 100 Stamina and 100 Magicka.
  Health excludes undead; Magicka retains Requiem's Atronach restriction.
- Predator's Might: +75 Health and +75 Stamina for 30 seconds.
- Elemental Fury: +0.30 weapon speed for 15 seconds.
- Become Ethereal: 5-second ethereal effect.
- Slow Time: magnitude 0.30 for 10 seconds.
- Aura Whisper: 10 seconds.
These are the first-word values, not the stronger second/third-word variants.
Local records do not establish the user's final winning values for every mod.
Dragon Aspect and Battle Fury records were unavailable: the supplied Requiem
ESP does not override them, and Dragonborn.esm and relevant override plugins
were not supplied. Their compatibility must be checked in game. The generic
self-cast path and diagnostics include them when their loaded first-word spell
uses self delivery. This package does not claim every buff shout is verified.
Detailed effect flags, conditions and attached scripts are included under
Documentation/Skald/BuffCastAudit/Requiem-self-buff-records.json.

PRESERVED IRON LUNGS
All three Unrelenting Force words are still granted and unlocked. Manual UF
retains its Stamina gate/cost, magic-damage bonus and absence of a new cooldown.
The drawback remains 20% more incoming physical damage for 3 seconds after
any manual shout, refreshing without stacking. Automatic Skald casts remain
exempt. No 50% other-shout cooldown penalty or regeneration lockout is added.
Echoing Steel and the other existing trait rules are preserved.

INSTALL AND CHECK
1. Exit Skyrim and replace the previous combined package in MO2.
2. Keep Biggie Traits - Combined.esp enabled at its existing load-order position.
3. Ensure BiggieTraitMechanics.dll wins conflicts and its SKSE log reports 1.4.3.
4. Load an existing save; removing and reselecting the trait is not required.
5. Store Kyne's Peace, reduce the player resources, and use a melee power attack
   when Skald is ready. Check resource restoration and the Skald diagnostic lines.
6. Check Predator's Might, Elemental Fury and Dragon Aspect independently.
   Compare each with a manual first-word cast; do not compare with three words.
7. Verify offensive stored shouts, Skald recovery, and Iron Lungs still behave
   as before. Skald should not trigger Iron Lungs' manual-shout vulnerability.
If a buff still fails, retain BiggieTraitMechanics.log from that session so the
loaded spell, its effects and the before/after observations can be inspected.

VALIDATION / BETA
Packaging requires a successful Windows build, the five existing native rule
test suites, and source/DLL hashes matching the supplied build commit. Those
tests verify combat rules, not Skyrim's native self-buff casting. The complete
ESP (all 66 gameplay records and TES4 header) is byte-identical to v2.7.2-beta1.
Existing scripts, INIs, artwork and unrelated native sources are preserved.
There is still exactly one ESP. No in-game validation has been performed.

PREVIOUS RELEASE NOTES (historical)

'''

AUDIT_README = '''# Skald self-buff casting audit — 2.7.3-beta1

This candidate changes only the normal immediate-cast request and adds bounded,
read-only runtime observations. It does not substitute effects, rewrite shout
records, or force resource restoration. The exact source of the reported game
failure remains unconfirmed until the revised build is tested in Skyrim.

| Shout | Skyrim first-word spell | Value in supplied Requiem.esp |
| --- | --- | --- |
| Kyne's Peace | 00082A34 | Restore 100 Health, Stamina and Magicka |
| Predator's Might | 0009E0CC | +75 Health and Stamina for 30 seconds |
| Elemental Fury | 0002C595 | +0.30 weapon speed for 15 seconds |
| Become Ethereal | 0005F6EB | Ethereal for 5 seconds |
| Slow Time | 00048AD0 | Magnitude 0.30 for 10 seconds |
| Aura Whisper | 0008AFCC | 10 seconds |
| Dragon Aspect | Unavailable locally | Exact records not audited |
| Battle Fury | Unavailable locally | Exact records not audited |

The six inspected spells use VoicePower, Fire-and-Forget, Self delivery. These
values precede perk scaling and any later load-order overrides. Kyne's Peace
restoration is embedded in its first-word spell as native Value Modifier
effects. Predator's Might uses native Peak Value Modifier effects. Those
benefits do not require a script listening for a manual shout event.

Kyne's Health effect excludes undead, its Stamina effect has no conditions,
and its Magicka effect excludes actors with Skyrim spell 000E5F51. Those
conditions are preserved. Related animal-control and visual scripts are also
preserved. The package does not bypass restrictions or grant higher words.

`Requiem-self-buff-records.json` records all 29 matching self-delivery player
shout spell overrides found in the supplied plugin, with magnitudes, durations,
delivery, flags, conditions and VMAD script names. This is a record audit, not
an in-game compatibility test. Skyrim.esm, Dragonborn.esm and every winning
modlist override were not available; script implementations were not supplied.

Diagnostics inspect the loaded first-word spell rather than this static audit.
Each observed self cast records resource and matching active-effect snapshots
before, immediately afterward, and after 0.25 seconds of unpaused gameplay.
They do not change gameplay values. Regeneration, a power attack's own Stamina
cost and other mods can affect the observed differences. Test with missing
resources, compare equivalent first-word casts, and retain the session log.
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
    parser.add_argument('--audit', type=Path, default=AUDIT)
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
    for name in ('src/SkaldDiagnostics.h', 'src/SkaldRuntime.h', 'tests/rules_tests.cpp',
                 'tests/lab_tests.cpp', 'tests/skald_tests.cpp', 'tests/iron_lungs_tests.cpp',
                 'tests/overexertion_tests.cpp', 'src/IronLungsGrant.h', 'src/Overexertion.h'):
        assert name in info['source_sha256_lf'], name
    for name, digest in info['source_sha256_lf'].items():
        assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
    changed_native = {
        'src/main.cpp', 'src/SkaldRuntime.h', 'CMakeLists.txt',
        'tools/build_windows.ps1', 'README.md',
    }
    for name, data in old.items():
        if name.startswith(SOURCE_PREFIX):
            relative = name[len(SOURCE_PREFIX):]
            if relative not in changed_native:
                assert (NATIVE / relative).read_bytes() == data, relative
    audit = json.loads(args.audit.read_text())
    assert audit['sha256'] == REQUIEM_SHA, 'Unexpected record-audit source'
    assert len(audit['self_delivery_player_shout_spells']) == 29
    # Retain evidence provenance without archiving a transient workspace path.
    audit['source'] = 'Requiem.esp (supplied plugin; SHA-256 recorded below)'
    files[DLL] = native[DLL]
    for name, data in native.items():
        if not name.startswith('SKSE/'):
            files[BUILD_PREFIX + name] = data
    for path in NATIVE.rglob('*'):
        if path.is_file() and '_build' not in path.parts:
            files[SOURCE_PREFIX + path.relative_to(NATIVE).as_posix()] = path.read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files[DOCS + 'README.md'] = AUDIT_README.encode()
    files[DOCS + 'Requiem-self-buff-records.json'] = encode(audit)
    files[DOCS + 'v2_7_2-Baseline-Validation.json'] = old['Validation.json']
    for name in ('skald_buffs.py', 'esp.py'):
        files[DOCS + 'Rebuild/' + name] = (ROOT / name).read_bytes()
    files[DOCS + 'Rebuild/README.txt'] = (
        'REBUILD v2.7.3-beta1\n\n'
        'Run from this folder with Python 3 and keep the complete extracted\n'
        'package layout. The script finds the archived native sources and audit.\n'
        'Obtain the unchanged v2.7.2-beta1 baseline ZIP with SHA-256:\n'
        + BASE_SHA + '\n\n'
        'The Windows-built native ZIP must contain BuildInfo.json and the DLL\n'
        'matching all archived native source hashes and this source commit:\n'
        + args.source_commit + '\n\n'
        'python skald_buffs.py BASELINE.zip --verify-plugin-only\n'
        'python skald_buffs.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip '
        '--source-commit ' + args.source_commit + '\n\n'
        'The output must not already exist. Native build instructions are in\n'
        'Documentation/ThreeTraits/NativeSource/README.md. No ESP edits occur.\n'
    ).encode()
    allowed = {DLL, 'README.txt', 'Validation.json'}
    for name, data in old.items():
        if name not in allowed and not name.startswith((BUILD_PREFIX, SOURCE_PREFIX)):
            assert files[name] == data, name
    assert all(files[n] == data for n, data in old.items() if n.lower().endswith(('.esp', '.esm', '.esl', '.pex', '.psc', '.ini', '.dds')))
    report.update(
        version=VERSION, native_build=info, in_game_tested=False,
        change='Skald CastSpellImmediate second argument true to false; normal immediate cast candidate',
        confirmed_root_cause=False, all_buff_shouts_in_game_verified=False,
        exact_loaded_first_word_preserved=True, native_magnitude_override=0,
        skald_recovery_seconds_by_base_speech={'0': 10, '50': 6, '100': 3},
        diagnostics='bounded read-only self-cast effect/AV snapshots before, immediately after and at 0.25 seconds',
        record_audit_source_sha256=audit['sha256'],
        record_audit_self_spell_overrides=29,
        exact_records_unavailable=['Dragon Aspect', 'Battle Fury'],
        all_iron_lungs_benefits_costs_and_drawback_preserved=True,
        full_word_grant_preserved=True, other_trait_rules_preserved=True,
        papyrus_scripts_unchanged=True, all_thumbnails_preserved_from_v2_7_2=True,
        native_source_changes=sorted(changed_native),
        new_native_sources=['src/SkaldDiagnostics.h'],
        existing_native_rule_suites=5,
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
