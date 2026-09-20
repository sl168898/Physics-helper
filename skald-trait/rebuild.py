"""Build the complete v2.6.0 Skald update from v2.5.0 and a verified Windows build."""
from pathlib import Path
import argparse
import base64
import hashlib
import json
import struct
import sys
from zipfile import ZipFile, ZIP_DEFLATED

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(ROOT / 'venom-harvester/tools'))
from esp import records, fields, field, record, group

sha = lambda b: hashlib.sha256(b).hexdigest()
js = lambda o: (json.dumps(o, indent=2) + '\n').encode()
text = lambda t: t.encode() + b'\0'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', type=Path, required=True)
    ap.add_argument('--native', type=Path)
    ap.add_argument('--out', type=Path)
    ap.add_argument('--verify-plugin-only', action='store_true')
    args = ap.parse_args()
    with ZipFile(args.base) as z:
        assert z.testzip() is None
        old = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    manifest = json.loads(old['Validation.json'])
    assert manifest['version'] == '2.5.0'
    assert sha(args.base.read_bytes()) == '18f3b19546d74873a5357640dcc3a576d9318ec86da2d7e7e52135ced4f9efde'
    assert all(sha(old[n]) == digest for n, digest in manifest['files'].items())
    files = dict(old)
    name = 'Biggie Traits - Combined.esp'
    rs = list(records(old[name]))
    masters = [v for t, v in fields(rs[0][1]) if t == b'MAST']
    own = len(masters) << 24
    byid = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in rs}
    ability, marker, power, effect = [own | i for i in (0xB00, 0xB01, 0xB06, 0xB07)]
    assert power not in byid and effect not in byid
    assert dict(fields(byid[ability][1]))[b'EDID'] == b'Traits_SkaldAb\0'
    desc = 'Gain a lesser power to store an unlocked shout. Melee power attacks release its first word, once every <10 seconds> (<6> at Speech 50, <3> at 100). Light melee attacks deal <25% less damage>.'
    marker_data = bytearray(dict(fields(byid[own | 0xF21][1]))[b'DATA'])
    assert len(marker_data) == 152 and struct.unpack_from('<I', marker_data, 64)[0] == 1
    # Neutral constant-effect marker; discard Echoing Steel's attached perk.
    struct.pack_into('<I', marker_data, 136, 0)
    groups = {}
    for h, b in rs[1:]:
        fid = struct.unpack_from('<I', h, 12)[0]
        fs = list(fields(b))
        if fid == ability:
            assert [struct.unpack('<I', v)[0] for t, v in fs if t == b'EFID'] == [marker, own | 0xB05]
            # The perk-backed penalty effect is unchanged; only cloak radius
            # becomes zero after converting its separate marker to a script archetype.
            b = b''.join(field(t, text(desc) if t == b'DESC' else struct.pack('<fII', 0, 0, 0) if t == b'EFIT' else v) for t, v in fs)
        elif fid == marker:
            b = b''.join(field(t, bytes(marker_data) if t == b'DATA' else text(desc.replace('<', '').replace('>', '')) if t == b'DNAM' else v) for t, v in fs)
        groups.setdefault(h[:4], []).append(record(h[:4], fid, b, head=h))
    # Copy the known working lesser-power layout, keeping only its vanilla
    # equip slot and neutral self-cast effect data. No VMAD or new master.
    power_fs = list(fields(byid[own | 0x921][1]))
    spit = bytearray(dict(power_fs)[b'SPIT'])
    assert len(spit) == 36 and struct.unpack_from('<III', spit, 8)[0] == 3
    struct.pack_into('<I', spit, 4, 1)  # explicit zero magicka cost
    new_power = b''.join(field(t, {
        b'EDID': text('BT_SkaldStoreShoutPower'), b'FULL': text('Skald - Store Shout'),
        b'DESC': text('Choose an unlocked shout to store for Skald. Melee power attacks release its first word. Recovery is 10 seconds, 6 at base Speech 50, and 3 at 100.'),
        b'SPIT': bytes(spit), b'EFID': struct.pack('<I', effect),
    }.get(t, v)) for t, v in power_fs)
    effect_fs = list(fields(byid[own | 0x922][1]))
    new_effect = b''.join(field(t, {
        b'EDID': text('BT_SkaldStoreShoutEffect'), b'FULL': text('Skald - Store Shout'),
        b'DNAM': text('Choose and store an unlocked shout for Skald.'),
    }.get(t, v)) for t, v in effect_fs if t != b'VMAD')
    groups[b'SPEL'].append(record(b'SPEL', power, new_power))
    groups[b'MGEF'].append(record(b'MGEF', effect, new_effect))
    header = b''
    for t, v in fields(rs[0][1]):
        if t == b'HEDR':
            version, count, nxt = struct.unpack('<fII', v)
            assert count == len(rs) - 1 and nxt > 0xB07
            v = struct.pack('<fII', version, count + 2, nxt)
        header += field(t, v)
    files[name] = record(b'TES4', 0, header, head=rs[0][0]) + b''.join(group(t, b''.join(v)) for t, v in groups.items())
    cr = list(records(files[name]))
    cb = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in cr}
    assert len(cr) == len(rs) + 2 and len(cb) == len(cr)
    assert [v for t, v in fields(cr[0][1]) if t == b'MAST'] == masters
    assert struct.unpack_from('<I', cr[0][0], 8)[0] & 0x200
    for fid, original in byid.items():
        if fid not in (0, ability, marker):
            assert cb[fid] == original, hex(fid)
    assert cb[own | 0xB04] == byid[own | 0xB04] and cb[own | 0xB05] == byid[own | 0xB05]
    assert struct.unpack_from('<I', dict(fields(cb[marker][1]))[b'DATA'], 8)[0] == 0
    assert struct.unpack_from('<I', dict(fields(cb[marker][1]))[b'DATA'], 136)[0] == 0
    assert b'VMAD' not in dict(fields(cb[effect][1]))
    assert struct.unpack_from('<III', dict(fields(cb[power][1]))[b'SPIT'], 8)[0] == 3
    # This is the compiled compatibility stub whose disassembly is supplied.
    pex = base64.b64decode((HERE / 'BT_SkaldHitEffect.pex.base64').read_text())
    assert sha(pex) == 'e0473057afa9bd892e581fdd6a219af518b38fc34f1458ab3c3970dc708c4814'
    assert b'SetVoiceRecoveryTime' not in pex and b'OnHit' in pex
    files['Scripts/BT_SkaldHitEffect.pex'] = pex
    files['Source/Scripts/BT_SkaldHitEffect.psc'] = (HERE / 'papyrus/BT_SkaldHitEffect.psc').read_bytes()
    report = dict(version='2.6.0', trait='Skald', stored_first_word=True,
        lesser_power_local_formid='0xB06', base_speech_thresholds=[0, 50, 100],
        recovery_seconds=[10, 6, 3], light_melee_damage_multiplier=0.75,
        echo='following melee power attack within 5 seconds; 1.5x or 2x two-handed',
        old_cloak_removed=True, old_OnHit_handlers_empty=True, normal_shout_recovery_unchanged=True,
        saved_choice_and_recovery=True, new_masters=0, new_records=2,
        changed_existing_records=[hex(ability), hex(marker)],
        other_existing_records_unchanged=len(rs) - 3, in_game_tested=False,
        baseline_sha256=sha(args.base.read_bytes()), plugin_sha256=sha(files[name]), papyrus_sha256=sha(pex))
    if args.verify_plugin_only:
        print(json.dumps(report, indent=2)); return
    assert args.native and args.out, '--native and --out are required to package'
    with ZipFile(args.native) as z:
        assert z.testzip() is None
        native = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.3.0' and info['combined_version'] == '2.6.0'
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    dll_path = 'SKSE/Plugins/BiggieTraitMechanics.dll'
    assert native[dll_path][:2] == b'MZ' and sha(native[dll_path]) == info['dll_sha256']
    source_root = ROOT / 'three-traits/native'
    for n, digest in info['source_sha256_lf'].items():
        assert sha((source_root / n).read_bytes().replace(b'\r\n', b'\n')) == digest, n
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source_root / n).read_bytes()
    files[dll_path] = native[dll_path]
    for n, b in native.items():
        if not n.endswith('.dll'): files['Documentation/ThreeTraits/NativeBuild/' + n] = b
    for n in ['tools/build_windows.ps1', 'README.md']:
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source_root / n).read_bytes()
    for n in ['rebuild.py', 'README.txt', 'BT_SkaldHitEffect.pex.base64', 'PEX-Inspection.txt']:
        files['Documentation/Skald/Rebuild/' + n] = (HERE / n).read_bytes()
    files['Documentation/Skald/Rebuild/esp.py'] = Path(sys.modules['esp'].__file__).read_bytes()
    files['Documentation/Skald/Rebuild/Validation.json'] = js(report)
    files['Documentation/v2_5_0-Validation.json'] = old['Validation.json']
    files['README.txt'] = (HERE / 'README.txt').read_bytes() + old['README.txt']
    allowed = {name, dll_path, 'Scripts/BT_SkaldHitEffect.pex', 'Source/Scripts/BT_SkaldHitEffect.psc', 'README.txt', 'Validation.json'}
    for n, b in old.items():
        if not n.startswith('Documentation/') and n not in allowed: assert files[n] == b, n
    manifest.update(version='2.6.0', baseline=args.base.name, baseline_sha256=sha(args.base.read_bytes()),
                    validation=report, trait_mechanics_dll=info)
    manifest['files'] = {n: sha(b) for n, b in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = js(manifest)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.out, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for n, b in sorted(files.items()): z.writestr(n, b)
    with ZipFile(args.out) as z:
        assert z.testzip() is None
        assert all(sha(z.read(n)) == digest for n, digest in manifest['files'].items())
        assert sum(n.endswith('.esp') for n in z.namelist()) == 1
    print(json.dumps(dict(path=str(args.out.resolve()), sha256=sha(args.out.read_bytes()), validation=report), indent=2))


if __name__ == '__main__':
    main()
