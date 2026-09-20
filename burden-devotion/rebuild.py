"""Rebuild Burden of Devotion in the complete v2.4.2 package.

Usage: rebuild.py --base BASE.zip --ordinator ORDINATOR.zip --native NATIVE.zip --out OUT.zip
Use --verify-plugin-only before the Windows helper build is available.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys
from zipfile import ZipFile, ZIP_DEFLATED

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'venom-harvester/tools'))
from esp import records, fields, field, record, group

sha = lambda b: hashlib.sha256(b).hexdigest()
s = lambda tag, text: field(tag, text.encode() + b'\0')

README = '''BIGGIE TRAITS COMBINED v2.5.0 - BURDEN OF DEVOTION REBUILD

Burden of Devotion: shrine blessings are 50% stronger, but maximum carrying
capacity is reduced by 100. There is no carried-weight threshold and no
additional blessing duration. Other nine traits retain their existing rules.

INSTALL / EXISTING SAVES
1. Close Skyrim completely. Replace your previous combined package in MO2.
   This package must win both Biggie Traits - Combined.esp and
   SKSE/Plugins/BiggieTraitMechanics.dll. Keep only one combined ESP enabled.
2. Keep your original Biggie Traits and the existing dependencies enabled.
   Skyrim Steam 1.6.1170, matching SKSE and Address Library remain required.
3. If Burden of Devotion was already selected, remove and reselect it once
   through your trait menu. This refreshes the saved carry-weight effect and
   attaches the new blessing perk. The final penalty is 100 total, not 150.
4. Obtain a fresh shrine blessing. Previously active blessings are not
   rewritten. Use a test save if your trait-menu setup restricts reselection.

BEHAVIOR
The bonus multiplies the numerical magnitude of received MagicBlessing
effects by 1.5 through Skyrim's native incoming-spell-magnitude perk entry.
It applies at any carried weight. It does not strengthen unrelated spells,
potions, ordinary priest healing, or permanent deity follower abilities.
Non-numeric effects such as curing diseases cannot become 50% stronger.
Duration is unchanged. Other perk multipliers and display rounding still apply.
Shrine mods must retain the standard MagicBlessing keyword, as required by
other blessing perks. The keyword is tested on the incoming magic item using
the same HasKeyword condition as Ordinator's Pilgrim perk. Ordinator itself
is NOT a dependency. No spell or magic-effect records from other mods are
overridden, so their shrine effects and balance changes remain in place.

The old helper's active-effect hooks for Burden of Devotion have been removed.
Install the updated DLL as well as the ESP; leaving the old DLL would retain
the previous conditional 25% strength / double-duration adjustment.
The helper still supplies the existing combat-trait and Lab Skeever mechanics.
There is no new shrine polling timer, script or background inventory scan.

VALIDATION
The package verifies its emitted perk against the entry-point layout of
Ordinator's incoming-healing perk and the MagicBlessing condition of Pilgrim.
All existing FormIDs and masters are retained. Only the Burden ability/effect
records change; one private perk is added. Other gameplay records, Papyrus
scripts, thumbnails and VenomHarvester.dll are preserved byte for byte.
The Windows DLL build and existing combat/Lab Skeever tests must pass before
packaging. This update has not been tested inside Skyrim.

IN-GAME CHECK
Compare the same freshly obtained numerical shrine blessing with the trait
off and on: a base 20-point bonus should become 30, with the same duration.
Repeat above half carrying capacity: it should still be 30. Verify that
maximum capacity falls by 100 and returns when the trait is removed.
Some descriptions use fixed text; compare actual Active Effects and character
statistics where possible. If the bonus is absent, report the specific shrine
and upload BiggieTraitMechanics.log from that session.

----- Historical release notes follow; the rules above supersede old Burden rules -----

'''


def entry_blocks(fs):
    result, active = [], None
    for tag, value in fs:
        if tag == b'PRKE':
            assert active is None
            active = []
        if active is not None:
            active.append((tag, value))
            if tag == b'PRKF':
                result.append(active)
                active = None
    assert active is None
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', required=True, type=Path)
    ap.add_argument('--ordinator', required=True, type=Path)
    ap.add_argument('--native', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--verify-plugin-only', action='store_true')
    args = ap.parse_args()
    with ZipFile(args.base) as z:
        assert z.testzip() is None
        old = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    manifest = json.loads(old['Validation.json'])
    assert manifest['version'] == '2.4.2'
    assert all(sha(old[n]) == h for n, h in manifest['files'].items())
    files = dict(old)
    name = 'Biggie Traits - Combined.esp'
    rs = list(records(old[name]))
    masters = [v for t, v in fields(rs[0][1]) if t == b'MAST']
    own = len(masters) << 24
    byid = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in rs}
    ability, effect, perkid = own | 0xF00, own | 0xF01, own | 0xF02
    assert perkid not in byid
    assert dict(fields(byid[ability][1]))[b'EDID'] == b'Traits_BurdenOfDevotionAb\0'
    # Verify the native incoming magnitude layout and blessing selector against
    # real known records. Reuse only the vanilla keyword condition (no new master).
    with ZipFile(args.ordinator) as z:
        n = next(n for n in z.namelist() if n.endswith('Ordinator - Perks of Skyrim.esp'))
        refs = {dict(fields(b)).get(b'EDID'): list(fields(b)) for h, b in records(z.read(n)) if h[:4] == b'PERK'}
    pilgrim = entry_blocks(refs[b'ORD_Res50_Pilgrim_Perk_50\0'])[0]
    selector = next(v for t, v in pilgrim if t == b'CTDA')
    assert len(selector) == 32
    assert struct.unpack_from('<f', selector, 4)[0] == 1
    assert struct.unpack_from('<II', selector, 8) == (560, 0xFB98C)
    assert (b'PRKC', b'\1') in pilgrim
    healing = entry_blocks(refs[b'ORD_Alc80_Witchmaster_Perk_80_Proc_DoubleHealing\0'])[0]
    assert (b'DATA', bytes([41, 3, 2])) in healing
    assert (b'PRKC', b'\1') in healing and (b'EPFT', b'\1') in healing
    perk = s(b'EDID', 'BT_BurdenOfDevotionBlessingPerk') + s(b'DESC', '')
    perk += field(b'DATA', bytes([0, 0, 1, 1, 0]))
    perk += field(b'PRKE', bytes([2, 0, 0])) + field(b'DATA', bytes([41, 3, 2]))
    perk += field(b'PRKC', b'\1') + field(b'CTDA', selector)
    perk += field(b'EPFT', b'\1') + field(b'EPFD', struct.pack('<f', 1.5)) + field(b'PRKF', b'')
    groups = {}
    for h, b in rs[1:]:
        fid = struct.unpack_from('<I', h, 12)[0]
        if fid == ability:
            fs = list(fields(b))
            assert [v for t, v in fs if t == b'EFID'] == [struct.pack('<I', effect)]
            assert [v for t, v in fs if t == b'EFIT'] == [struct.pack('<fII', 50, 0, 0)]
            b = b''.join(field(t, b'Shrine blessings you receive are <50% stronger>, but your maximum carrying capacity is reduced by <100>.\0' if t == b'DESC' else struct.pack('<fII', 100, 0, 0) if t == b'EFIT' else v) for t, v in fs)
        elif fid == effect:
            fs = list(fields(b)); data = bytearray(dict(fs)[b'DATA'])
            assert len(data) == 152
            assert struct.unpack_from('<II', data, 64) == (0, 32)
            assert struct.unpack_from('<I', data, 0)[0] & 4  # detrimental carry effect
            assert struct.unpack_from('<I', data, 136)[0] == 0
            struct.pack_into('<I', data, 136, perkid)
            b = b''.join(field(t, bytes(data) if t == b'DATA' else b'Shrine blessings are 50% stronger. Maximum carrying capacity is reduced by 100.\0' if t == b'DNAM' else v) for t, v in fs)
        groups.setdefault(h[:4], []).append(record(h[:4], fid, b, head=h))
    groups[b'PERK'].append(record(b'PERK', perkid, perk))
    header = b''
    for t, v in fields(rs[0][1]):
        if t == b'HEDR':
            ver, count, nxt = struct.unpack('<fII', v)
            assert count == len(rs) - 1 and nxt > 0xF02
            v = struct.pack('<fII', ver, count + 1, nxt)
        header += field(t, v)
    files[name] = record(b'TES4', 0, header, head=rs[0][0]) + b''.join(group(t, b''.join(v)) for t, v in groups.items())
    cr = list(records(files[name]))
    cb = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in cr}
    assert len(cr) == len(rs) + 1 and len(cb) == len(cr)
    for fid, r in byid.items():
        if fid not in (0, ability, effect):
            assert cb[fid] == r, hex(fid)
    assert [v for t, v in fields(cr[0][1]) if t == b'MAST'] == masters
    assert struct.unpack_from('<I', cr[0][0], 8)[0] & 0x200
    af, ef = dict(fields(cb[ability][1])), dict(fields(cb[effect][1]))
    assert af[b'EFIT'] == struct.pack('<fII', 100, 0, 0)
    assert struct.unpack_from('<I', ef[b'DATA'], 136)[0] == perkid
    blocks = entry_blocks(list(fields(cb[perkid][1])))
    assert len(blocks) == 1
    assert dict(blocks[0])[b'DATA'] == bytes([41, 3, 2])
    assert dict(blocks[0])[b'CTDA'] == selector
    assert dict(blocks[0])[b'EPFD'] == struct.pack('<f', 1.5)
    report = {'version': '2.5.0', 'trait': 'Burden of Devotion', 'carry_capacity_penalty': 100,
              'blessing_magnitude_multiplier': 1.5, 'entry_point': 'Mod Incoming Spell Magnitude (41)',
              'blessing_keyword': 'Skyrim.esm|000FB98C', 'weight_condition': False,
              'duration_bonus': False, 'new_records': 1, 'new_masters': 0,
              'changed_existing_records': [hex(ability), hex(effect)],
              'other_existing_records_unchanged': len(rs) - 3, 'in_game_tested': False,
              'baseline_sha256': sha(args.base.read_bytes()), 'plugin_sha256': sha(files[name])}
    if args.verify_plugin_only:
        print(json.dumps(report, indent=2))
        return
    assert args.native and args.out, '--native and --out are required to create an installable package'
    with ZipFile(args.native) as z:
        assert z.testzip() is None
        native = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.2.0' and info['combined_version'] == '2.5.0'
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    dll = native['SKSE/Plugins/BiggieTraitMechanics.dll']
    assert dll[:2] == b'MZ' and sha(dll) == info['dll_sha256']
    source_root = Path(__file__).resolve().parents[1] / 'three-traits/native'
    for n, digest in info['source_sha256_lf'].items():
        assert sha((source_root / n).read_bytes().replace(b'\r\n', b'\n')) == digest, n
    source = (source_root / 'src/main.cpp').read_text()
    assert 'installBlessingHooks' not in source and 'BlessingHook' not in source
    assert 'lightLoad' not in source and 'ae->duration' not in source
    files['SKSE/Plugins/BiggieTraitMechanics.dll'] = dll
    for n, b in native.items():
        if not n.endswith('.dll'):
            files['Documentation/ThreeTraits/NativeBuild/' + n] = b
    for n in info['source_sha256_lf']:
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source_root / n).read_bytes()
    for n in ('tools/build_windows.ps1', 'README.md'):
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source_root / n).read_bytes()
    files['README.txt'] = README.encode() + old['README.txt']
    files['Documentation/BurdenOfDevotion/rebuild.py'] = Path(__file__).read_bytes()
    files['Documentation/BurdenOfDevotion/esp.py'] = Path(sys.modules['esp'].__file__).read_bytes()
    files['Documentation/BurdenOfDevotion/Validation.json'] = json.dumps(report, indent=2).encode()
    files['Documentation/v2_4_2-Validation.json'] = old['Validation.json']
    manifest.update(version='2.5.0', baseline=args.base.name, baseline_sha256=sha(args.base.read_bytes()),
                    validation=report, trait_mechanics_dll=info)
    manifest['files'] = {n: sha(b) for n, b in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = json.dumps(manifest, indent=2).encode()
    for n, b in old.items():
        if n.startswith(('Documentation/',)) or n in (name, 'README.txt', 'Validation.json', 'SKSE/Plugins/BiggieTraitMechanics.dll'):
            continue
        assert files[n] == b, n
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.out, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for n, b in sorted(files.items()):
            z.writestr(n, b)
    with ZipFile(args.out) as z:
        assert z.testzip() is None
        assert all(sha(z.read(n)) == digest for n, digest in manifest['files'].items())
        assert sum(n.endswith('.esp') for n in z.namelist()) == 1
    print(json.dumps({'path': str(args.out.resolve()), 'sha256': sha(args.out.read_bytes()), 'validation': report}, indent=2))


if __name__ == '__main__':
    main()
