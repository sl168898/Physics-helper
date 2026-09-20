"""Package the Skald menu callback fix without changing the existing ESP.

Usage: package_menu_fix.py --base v2_6_0.zip --native native.zip --out v2_6_1.zip
"""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json

ROOT = Path(__file__).resolve().parents[1]
sha = lambda b: hashlib.sha256(b).hexdigest()
js = lambda value: (json.dumps(value, indent=2) + '\n').encode()

NOTES = '''BIGGIE TRAITS COMBINED v2.6.1 - SKALD STORE SHOUT FIX

The Store Shout menu used the wrong button index offset. Every selected
button was shifted by four positions, so clicking a shout could instead
choose another shout, navigate, clear/cancel, or be silently ignored. This
explains a menu appearing without the "Skald stored" confirmation and leaves
power attacks with no stored shout to release. The offset is now zero.

INSTALL AND RETEST
1. Close Skyrim and replace your previous combined package in MO2 with this
   complete archive. Its BiggieTraitMechanics.dll must win file conflicts.
2. Load your save and use Skald - Store Shout again. Select Fire Breath.
3. Confirm "Skald stored: Fire Breath" appears. Reopen the power if you wish:
   the menu's Current line should show Fire Breath. Then close the menu.
4. Perform a melee power attack. Skald should release the first-word effect.
   Base Speech still gives 10/6/3 seconds of recovery at below 50/50/100.

No trait reselection or new game is required. Store the shout again even if
you previously clicked its name: the old index bug may have saved no choice
or a different one. If an existing cooldown is running, let it finish.

The log now records menu opening, returned button indices, their mapped
actions, selection success/failure and cast requests. A power attack with
no stored shout logs once per selection/load. If the problem persists,
send BiggieTraitMechanics.log from that test session before restarting the
game. Its first line should report helper version 1.3.1.

Skald's drawback, cooldowns, first-word cast path, saved data format, and
Echoing Steel's NEXT-power-attack synergy are unchanged. All ESP records,
Papyrus scripts, other gameplay assets and VenomHarvester.dll are preserved
byte for byte from v2.6.0. Skyrim Steam 1.6.1170 and the same dependencies
remain required.

The Windows build and all three existing rule suites passed before packaging.
Those tests cover the combat/cooldown rules, not Skyrim's live UI. The native
menu correction still needs the in-game retest above.

Technical reference for the formerly unnamed byte at offset 0x4C:
https://github.com/adya/CommonLibSSE/blob/3adc3270274f954caebc165ddcc7a3969596eb1e/include/RE/M/MessageBoxData.h

----- Previous release documentation -----

'''


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--base', type=Path, required=True)
    ap.add_argument('--native', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    args = ap.parse_args()
    assert sha(args.base.read_bytes()) == '3891fe022c1673eeb9001070fa4e615dd424496c5ca792096738e260949feb7d'
    with ZipFile(args.base) as z:
        assert z.testzip() is None
        old = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    manifest = json.loads(old['Validation.json'])
    assert manifest['version'] == '2.6.0'
    assert all(sha(old[n]) == digest for n, digest in manifest['files'].items())
    files = dict(old)
    with ZipFile(args.native) as z:
        assert z.testzip() is None
        native = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.3.1' and info['combined_version'] == '2.6.1'
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    assert info['source_commit'] == '7b0125f705e6b7c654a07948b1e536d35530ea7f'
    dll_path = 'SKSE/Plugins/BiggieTraitMechanics.dll'
    assert native[dll_path][:2] == b'MZ' and sha(native[dll_path]) == info['dll_sha256']
    source_root = ROOT / 'three-traits/native'
    for n, digest in info['source_sha256_lf'].items():
        data = (source_root / n).read_bytes()
        assert sha(data.replace(b'\r\n', b'\n')) == digest, n
        files['Documentation/ThreeTraits/NativeSource/' + n] = data
    files[dll_path] = native[dll_path]
    for n, data in native.items():
        if not n.endswith('.dll'):
            files['Documentation/ThreeTraits/NativeBuild/' + n] = data
    for n in ['README.md', 'tools/build_windows.ps1']:
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source_root / n).read_bytes()
    files['README.txt'] = NOTES.encode() + old['README.txt']
    files['Documentation/Skald/StoreMenuFix/README.txt'] = NOTES.encode()
    files['Documentation/Skald/StoreMenuFix/package_menu_fix.py'] = Path(__file__).read_bytes()
    files['Documentation/v2_6_0-Validation.json'] = old['Validation.json']
    changed = sorted(n for n, data in old.items()
        if not n.startswith('Documentation/') and n not in ('README.txt', 'Validation.json')
        and files[n] != data)
    assert changed == [dll_path], changed
    assert files['Biggie Traits - Combined.esp'] == old['Biggie Traits - Combined.esp']
    report = dict(version='2.6.1', corrected_button_offset=0, previous_button_offset=4,
        preserves_native_warning_type=True, changed_gameplay_files=changed,
        plugin_byte_identical=True, papyrus_byte_identical=True,
        new_records=0, new_masters=0, in_game_tested=False,
        baseline_sha256=sha(args.base.read_bytes()), native_build=info)
    files['Documentation/Skald/StoreMenuFix/Validation.json'] = js(report)
    manifest.update(version='2.6.1', baseline=args.base.name, baseline_sha256=sha(args.base.read_bytes()),
        validation=report, trait_mechanics_dll=info)
    manifest['files'] = {n: sha(data) for n, data in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = js(manifest)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.out, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for n, data in sorted(files.items()): z.writestr(n, data)
    with ZipFile(args.out) as z:
        assert z.testzip() is None
        assert all(sha(z.read(n)) == digest for n, digest in manifest['files'].items())
        assert sum(n.endswith('.esp') for n in z.namelist()) == 1
    print(json.dumps(dict(path=str(args.out.resolve()), sha256=sha(args.out.read_bytes()),
        bytes=args.out.stat().st_size, changed_gameplay_files=changed), indent=2))


if __name__ == '__main__':
    main()
