"""Update the 2.7.1-beta1 combined package to 2.7.2-beta1.

python overexertion.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip --source-commit SHA
python overexertion.py BASELINE.zip --verify-plugin-only
Native source lives beside this repository, or in the extracted package's
Documentation/ThreeTraits/NativeSource directory when using the archived copy.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile
from esp import records, fields, field

VERSION = '2.7.2-beta1'
SOURCE_COMMIT = 'eaaea1f8ad334e64ee959b6272d6361a2fa98b3d'
BASE_SHA = '5e916fd84153d47240be21c930de20b24989e1b4432a2b32e175d972ddb13640'
PLUGIN = 'Biggie Traits - Combined.esp'
DLL = 'SKSE/Plugins/BiggieTraitMechanics.dll'
SOURCE_PREFIX = 'Documentation/ThreeTraits/NativeSource/'
BUILD_PREFIX = 'Documentation/ThreeTraits/NativeBuild/'
DOCS = 'Documentation/IronLungs/Overexertion/'
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[2] / 'ThreeTraits/NativeSource'

README = '''BIGGIE TRAITS COMBINED v2.7.2-beta1 — IRON LUNGS OVEREXERTION

IRON LUNGS DRAWBACK
After manually shouting, take 20% more physical damage for 3 seconds.
This applies after any manual shout while Iron Lungs is selected. Another
manual shout refreshes the three-second window; the penalty does not stack.
Skald's automatic shouts do not trigger or refresh it. An Unrelenting Force
attempt blocked by insufficient Stamina does not trigger it.

The penalty affects incoming physical hits, including melee, unarmed/creature
attacks, arrows and bashes. Separate spell damage, poison and weapon enchantment
effects are not increased. Paused menus pause the timer. The temporary window
clears on loading a save, starting a new game, death or removing Iron Lungs.

The previously discussed 50% longer cooldown for other shouts is not included.
No Stamina-regeneration lockout has been added.

PRESERVED BENEFITS
Choosing Iron Lungs still grants Fus, Ro and Dah fully unlocked without spending
dragon souls. Existing selected-trait saves receive missing words after loading
and closing paused menus. Learned words remain after the trait is removed.
Manual Unrelenting Force still has no new shout cooldown, costs 25% of maximum
Stamina adjusted by shout recovery (minimum 10%), and adds magic damage equal
to 25% of current Stamina before payment. Sufficient Stamina is required.
Shout-power scaling, Skald's exemptions, Echoing Steel synergy and the
female-warrior-versus-giant thumbnail are preserved.

INSTALL
1. Exit Skyrim and replace the previous combined package in MO2.
2. Keep Biggie Traits - Combined.esp enabled at the same load-order position.
3. Ensure this ESP and BiggieTraitMechanics.dll win conflicts. The native helper
   must report version 1.4.2 in BiggieTraitMechanics.log.
4. Load your save. Existing Iron Lungs users need not remove and reselect it.

VALIDATION / BETA
Windows compilation and all native rule tests must pass before packaging.
Build provenance and source hashes must match the supplied source commit.
The ESP comparison permits changes only to Iron Lungs' trait-menu and Active
Effects description fields. The other 64 gameplay records, TES4 header, form
IDs, masters, scripts, INIs, artwork and unrelated native sources are preserved.
There is still exactly one ESP. Automated checks cannot replace Skyrim testing;
this release remains a beta requiring an in-game check.

IN-GAME CHECK
On a test save with Iron Lungs selected, compare the same incoming physical hit
before shouting, immediately after a manual shout, and after three seconds.
Confirm the brief 20% increase; shout again to check refresh without stacking.
Test both Unrelenting Force and another manual shout. Test Skald separately:
an automatic cast must not start or refresh the vulnerability window. Check
that a blocked UF attempt does not start it, and that magic/poison damage is
unchanged. Confirm trait removal, death and loading clear the transient window.
Check the updated description and that the full-word grant and normal UF
Stamina/magic-damage behavior remain available.

PREVIOUS RELEASE NOTES (historical)

'''


def sha(data):
    return hashlib.sha256(data).hexdigest()


def encode(value):
    return (json.dumps(value, indent=2) + '\n').encode()


def patch_plugin(original):
    before = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in records(original)}
    texts = {
        0x21000F30: (b'DESC',
            'Learn and unlock <all three words of Unrelenting Force>. It has <no cooldown>, '
            'costs <25% of maximum Stamina> and deals extra <magic damage equal to 25% of current Stamina>. '
            'Cooldown reduction lowers the cost, to a <10% minimum>. Requires enough Stamina. '
            'After any manual shout, take <20% more physical damage for 3 seconds>. '
            'Further shouts refresh this duration. Skald casts are exempt.'),
        0x21000F31: (b'DNAM',
            'Grants all three words of Unrelenting Force, fully unlocked. Unrelenting Force: no cooldown; '
            'costs 25% maximum Stamina, adjusted by shout recovery (minimum 10%); adds magic damage equal '
            'to 25% of current Stamina before payment. Requires enough Stamina. '
            'After any manual shout, take 20% more physical damage for 3 seconds. '
            'Further shouts refresh the duration without stacking. Skald casts are exempt.')
    }
    changed = set()

    def rewrite(blob):
        out = bytearray()
        pos = 0
        while pos < len(blob):
            header = bytearray(blob[pos:pos + 24])
            size = struct.unpack_from('<I', header, 4)[0]
            if header[:4] == b'GRUP':
                payload = rewrite(blob[pos + 24:pos + size])
                struct.pack_into('<I', header, 4, len(payload) + 24)
                out += header + payload
                pos += size
                continue
            formid = struct.unpack_from('<I', header, 12)[0]
            payload = blob[pos + 24:pos + 24 + size]
            if formid in texts:
                assert not struct.unpack_from('<I', header, 8)[0] & 0x40000
                tag, text = texts[formid]
                parts = list(fields(payload))
                assert sum(t == tag for t, _ in parts) == 1
                payload = b''.join(field(t, text.encode() + b'\0' if t == tag else v) for t, v in parts)
                struct.pack_into('<I', header, 4, len(payload))
                changed.add(formid)
            out += header + payload
            pos += 24 + size
        assert pos == len(blob)
        return bytes(out)

    result = rewrite(original)
    after = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in records(result)}
    assert changed == set(texts) and before.keys() == after.keys()
    for fid, (head, payload) in before.items():
        new_head, new_payload = after[fid]
        if fid not in texts:
            assert after[fid] == (head, payload), f'{fid:08X}'
        else:
            assert head[:4] == new_head[:4] and head[8:] == new_head[8:]
            tag, _ = texts[fid]
            assert [(t, v) for t, v in fields(payload) if t != tag] == [(t, v) for t, v in fields(new_payload) if t != tag]
    other_count = len(before) - len(texts) - 1
    assert other_count == 64
    return result, {
        'changed_record_fields': {'21000F30': 'DESC', '21000F31': 'DNAM'},
        'other_gameplay_records_byte_identical': other_count,
        'tes4_header_byte_identical': True, 'formids_unchanged': True,
        'new_records': 0, 'new_masters': 0, 'esl_flag_preserved': True,
    }


def read_zip(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        names = [n for n in archive.namelist() if not n.endswith('/')]
        assert len(names) == len(set(names)), 'Duplicate ZIP members'
        return {n.replace('\\', '/'): archive.read(n) for n in names}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('native_build', type=Path, nargs='?')
    parser.add_argument('output', type=Path, nargs='?')
    parser.add_argument('--source-commit', default=SOURCE_COMMIT,
                        help='Exact native build source commit; defaults to the verified release commit')
    parser.add_argument('--verify-plugin-only', action='store_true')
    args = parser.parse_args()
    assert sha(args.baseline.read_bytes()) == BASE_SHA, 'Wrong baseline package'
    old = read_zip(args.baseline)
    previous = json.loads(old['Validation.json'])
    for name, digest in previous['files'].items():
        assert sha(old[name]) == digest, name
    files = dict(old)
    files[PLUGIN], report = patch_plugin(old[PLUGIN])
    if args.verify_plugin_only:
        print(json.dumps(report, indent=2))
        return
    if not args.native_build or not args.output or not re.fullmatch(r'[0-9a-f]{40}', args.source_commit or ''):
        parser.error('Packaging requires NATIVE_BUILD.zip OUTPUT.zip and --source-commit SHA')
    native = read_zip(args.native_build)
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.4.2' and info['combined_version'] == VERSION
    assert info['source_commit'] == args.source_commit
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    assert native[DLL][:2] == b'MZ' and sha(native[DLL]) == info['dll_sha256']
    for name in ('src/Overexertion.h', 'tests/overexertion_tests.cpp', 'src/IronLungsGrant.h'):
        assert name in info['source_sha256_lf'], name
    for name, digest in info['source_sha256_lf'].items():
        assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
    changed_native = {
        'src/main.cpp', 'src/IronLungsRuntime.h', 'CMakeLists.txt',
        'tools/build_windows.ps1', 'README.md',
    }
    for name, data in old.items():
        if name.startswith(SOURCE_PREFIX):
            relative = name[len(SOURCE_PREFIX):]
            if relative not in changed_native:
                assert (NATIVE / relative).read_bytes() == data, relative
    files[DLL] = native[DLL]
    for name, data in native.items():
        if not name.startswith('SKSE/'):
            files[BUILD_PREFIX + name] = data
    for path in NATIVE.rglob('*'):
        if path.is_file() and '_build' not in path.parts:
            files[SOURCE_PREFIX + path.relative_to(NATIVE).as_posix()] = path.read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files[DOCS + 'v2_7_1-Baseline-Validation.json'] = old['Validation.json']
    for name in ('overexertion.py', 'esp.py'):
        files[DOCS + 'Rebuild/' + name] = (ROOT / name).read_bytes()
    files[DOCS + 'Rebuild/README.txt'] = (
        'REBUILD v2.7.2-beta1\n\n'
        'Run from this folder with Python 3. Keep the complete extracted package\n'
        'layout so the script can locate Documentation/ThreeTraits/NativeSource.\n'
        'Obtain the unchanged v2.7.1-beta1 baseline ZIP with SHA-256:\n'
        + BASE_SHA + '\n\n'
        'The Windows-built native ZIP must contain BuildInfo.json and the DLL,\n'
        'and must match all archived native source hashes. Its source commit is:\n'
        + args.source_commit + '\n\n'
        'python overexertion.py BASELINE.zip --verify-plugin-only\n'
        'python overexertion.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip '
        '--source-commit ' + args.source_commit + '\n\n'
        'The output file must not already exist. Native build instructions are\n'
        'in Documentation/ThreeTraits/NativeSource/README.md.\n'
    ).encode()
    allowed = {PLUGIN, DLL, 'README.txt', 'Validation.json'}
    for name, data in old.items():
        if name not in allowed and not name.startswith((BUILD_PREFIX, SOURCE_PREFIX)):
            assert files[name] == data, name
    assert all(files[n] == data for n, data in old.items() if n.lower().endswith(('.pex', '.psc', '.ini', '.dds')))
    report.update(
        version=VERSION, native_build=info, in_game_tested=False,
        drawback='20% more incoming physical damage for 3 seconds after any manual shout',
        repeated_shouts='refresh duration without stacking', skald_automatic_casts_exempt=True,
        insufficient_stamina_attempts_exempt=True, paused_menus_pause_timer=True,
        transient_window_clears_on=['save load', 'new game', 'death', 'trait removal'],
        other_shout_cooldown_penalty=False, stamina_regeneration_penalty=False,
        prior_iron_lungs_benefits_and_costs_preserved=True,
        full_word_grant_preserved=True, other_trait_rules_preserved=True,
        papyrus_scripts_unchanged=True, all_thumbnails_preserved_from_v2_7_1=True,
        native_source_changes=sorted(changed_native),
        new_native_sources=['src/Overexertion.h', 'tests/overexertion_tests.cpp'],
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
        for name, digest in manifest['files'].items():
            assert sha(archive.read(name)) == digest, name
    print(json.dumps({
        'output': str(args.output.resolve()), 'sha256': sha(args.output.read_bytes()),
        'bytes': args.output.stat().st_size, 'record_validation': report['changed_record_fields'],
        'other_records_preserved': report['other_gameplay_records_byte_identical'],
        'native_commit': info['source_commit'], 'in_game_tested': False,
    }, indent=2))


if __name__ == '__main__':
    main()
