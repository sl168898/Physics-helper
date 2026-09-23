"""Update the Art4 combined package to 2.7.1-beta1.

python grant_words.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip
python grant_words.py BASELINE.zip --verify-plugin-only
Native source lives beside this repository, or in the extracted package's
Documentation/ThreeTraits/NativeSource directory when using the archived copy.
"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile
from esp import records, fields, field

VERSION = '2.7.1-beta1'
BASE_SHA = '2396c7fc5b205a269ba49504a8fd86f967bcbd15b5db79a045188e607ad859a1'
PLUGIN = 'Biggie Traits - Combined.esp'
DLL = 'SKSE/Plugins/BiggieTraitMechanics.dll'
DOCS = 'Documentation/IronLungs/WordGrant/'
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[2] / 'ThreeTraits/NativeSource'

README = '''BIGGIE TRAITS COMBINED v2.7.1-beta1 — IRON LUNGS FULL SHOUT GRANT

Choosing Iron Lungs grants Unrelenting Force and teaches/unlocks all three of
its words (Fus, Ro, Dah) without spending dragon souls. Existing selected-trait
saves also receive any missing words after loading and leaving paused menus.
Already learned/unlocked words are skipped. Leave the menu and allow a moment
for the game to finish the sequence before checking the shout menu.

The words remain learned if Iron Lungs is later removed. The mod does not
alter quest stages or change your equipped shout. Only players with Iron Lungs
selected receive this grant. No new ESP, master, inventory item or script.

The trait-menu and Active Effects descriptions now mention the full shout grant.
The new female-warrior-versus-giant thumbnail is included. All other thumbnails
and the existing Stamina, magic-damage, Skald and Echoing Steel rules are kept.

INSTALL
1. Exit Skyrim and replace the previous combined package in MO2.
2. Keep Biggie Traits - Combined.esp enabled at the same load-order position.
3. Ensure this ESP, the Iron Lungs thumbnail and BiggieTraitMechanics.dll win
   conflicts. The helper must report version 1.4.1 in its SKSE log.
4. Load your save. Choose Iron Lungs if not already selected; existing users
   do not need to remove and reselect it. Close paused menus to allow the grant.

VALIDATION / BETA
Windows compilation and the four existing combat-rule suites must pass before
packaging. The ESP comparison checks that only Iron Lungs' two description
fields changed. Existing scripts/INIs, all other records and thumbnails are
preserved. This remains a beta requiring in-game verification of the new grant
and of the earlier Iron Lungs combat behavior.

IN-GAME CHECK
On a test save, select Iron Lungs with zero or partially learned UF words.
Close the menu and check that all three words become available without a soul
charge. Reload with the trait selected and check that there are no duplicate
rewards; known words should only be checked. Check another save without the
trait, then remove/reselect it. The in-game description should mention the grant.
BiggieTraitMechanics.log records each newly granted word and the final line:
'All three Unrelenting Force words verified unlocked'.

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
            'Cooldown reduction lowers the cost, to a <10% minimum>. Requires enough Stamina. Skald casts are exempt.'),
        0x21000F31: (b'DNAM',
            'Grants all three words of Unrelenting Force, fully unlocked. Unrelenting Force: no cooldown; '
            'costs 25% maximum Stamina, adjusted by shout recovery (minimum 10%); adds magic damage equal '
            'to 25% of current Stamina before payment. Requires enough Stamina. Skald casts are exempt.')
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
    return result, {
        'changed_record_fields': {'21000F30': 'DESC', '21000F31': 'DNAM'},
        'other_gameplay_records_byte_identical': len(before) - len(texts) - 1,
        'tes4_header_byte_identical': True, 'formids_unchanged': True,
        'new_records': 0, 'new_masters': 0, 'esl_flag_preserved': True,
    }


def main():
    baseline = Path(sys.argv[1])
    assert sha(baseline.read_bytes()) == BASE_SHA
    with zipfile.ZipFile(baseline) as archive:
        assert archive.testzip() is None
        old = {n: archive.read(n) for n in archive.namelist() if not n.endswith('/')}
    previous = json.loads(old['Validation.json'])
    for name, digest in previous['files'].items():
        assert sha(old[name]) == digest, name
    files = dict(old)
    files[PLUGIN], report = patch_plugin(old[PLUGIN])
    if sys.argv[2] == '--verify-plugin-only':
        print(json.dumps(report, indent=2)); return
    with zipfile.ZipFile(sys.argv[2]) as archive:
        assert archive.testzip() is None
        native = {n.replace('\\', '/'): archive.read(n) for n in archive.namelist() if not n.endswith('/')}
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.4.1' and info['combined_version'] == VERSION
    assert info['source_commit'] == '8bdc20bc79a90cae6bd18b2dba31740de63eb25d'
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    assert native[DLL][:2] == b'MZ' and sha(native[DLL]) == info['dll_sha256']
    assert 'src/IronLungsGrant.h' in info['source_sha256_lf']
    for name, digest in info['source_sha256_lf'].items():
        assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == digest, name
    for name in ['src/IronLungs.h', 'src/Skald.h', 'src/SkaldRuntime.h', 'src/Rules.h', 'src/LabVisit.h', 'src/EchoDiagnostics.h']:
        assert (NATIVE / name).read_bytes() == old['Documentation/ThreeTraits/NativeSource/' + name], name
    files[DLL] = native[DLL]
    for name, data in native.items():
        if not name.startswith('SKSE/'):
            files['Documentation/ThreeTraits/NativeBuild/' + name] = data
    for path in NATIVE.rglob('*'):
        if path.is_file() and '_build' not in path.parts:
            files['Documentation/ThreeTraits/NativeSource/' + path.relative_to(NATIVE).as_posix()] = path.read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files[DOCS + 'Art4-Baseline-Validation.json'] = old['Validation.json']
    for name in ['grant_words.py', 'esp.py']:
        files[DOCS + 'Rebuild/' + name] = (ROOT / name).read_bytes()
    allowed = {PLUGIN, DLL, 'README.txt', 'Validation.json'}
    for name, data in old.items():
        if name not in allowed and not name.startswith(('Documentation/ThreeTraits/NativeBuild/', 'Documentation/ThreeTraits/NativeSource/')):
            assert files[name] == data, name
    assert all(files[n] == data for n, data in old.items() if n.lower().endswith(('.pex', '.psc', '.ini', '.dds')))
    report.update(version=VERSION, native_build=info, in_game_tested=False,
                  grants='all three Unrelenting Force words, taught and unlocked',
                  dragon_soul_cost=0, synchronizes_existing_selected_saves=True,
                  learned_words_retained_after_trait_removal=True,
                  papyrus_scripts_unchanged=True, all_thumbnails_preserved_from_Art4=True,
                  previous_combat_rules_unchanged=True)
    files[DOCS + 'Validation.json'] = encode(report)
    manifest = {'version': VERSION, 'baseline': baseline.name, 'baseline_sha256': BASE_SHA,
                'traits': previous['traits'], 'validation': report, 'trait_mechanics_dll': info,
                'artwork_revision': 4, 'artwork_validation': previous['artwork_validation']}
    manifest['files'] = {n: sha(data) for n, data in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = encode(manifest)
    output = Path(sys.argv[3])
    if output.exists():
        raise FileExistsError(output)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert sum(n.lower().endswith(('.esp', '.esm', '.esl')) for n in archive.namelist()) == 1
        for name, digest in manifest['files'].items():
            assert sha(archive.read(name)) == digest, name
    print(json.dumps({'output': str(output.resolve()), 'sha256': sha(output.read_bytes()),
                      'bytes': output.stat().st_size, 'record_validation': report['changed_record_fields'],
                      'other_records_preserved': report['other_gameplay_records_byte_identical'],
                      'native_commit': info['source_commit'], 'in_game_tested': False}, indent=2))


if __name__ == '__main__':
    main()
