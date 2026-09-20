"""Build the full v2.6.2 diagnostic package from the unchanged v2.6.1 baseline."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import argparse
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]
SOURCE_COMMIT = '57615b7502dfbb5b4aa55fd64cc40969dd30c239'
BASE_SHA = '0b33a1839baf5372e1c4e9babe30d66eb0191dd465b95318e6173e9b6f28be76'
sha = lambda b: hashlib.sha256(b).hexdigest()
js = lambda v: (json.dumps(v, indent=2) + '\n').encode()

NOTES = '''BIGGIE TRAITS COMBINED v2.6.2 - ECHOING STEEL DIAGNOSTICS

This build makes Echoing Steel's runtime behavior observable. The source
audit and automated combat-rule tests did not establish a rule failure.
The previous user log showed successful startup only, with no hit evidence.
This is a diagnostic update, not a claim that the trait was verified in game.

INSTALL
Close Skyrim and replace the previous combined package in MO2 with this
complete archive. BiggieTraitMechanics.dll AND BiggieTraitMechanics.ini must
win file conflicts. Existing saves and trait selections remain usable.
Runtime requirement: Skyrim Steam 1.6.1170, with the same SKSE/dependencies.

SHORT DIAGNOSTIC CHECK
1. Load your save with Echoing Steel selected. A diagnostic capture message
   should appear. If the message says the trait is not selected, select it
   using your normal trait menu.
2. Shout manually, then land a melee power attack within FIVE seconds.
   You should see "Echoing Steel: primed (5 seconds)." and then
   "Echoing Steel: bonus applied to hit."
3. To check Skald's synergy, store a shout and let Skald's own recovery end.
   Power attack once to release its shout, then land ANOTHER power attack
   within five seconds. That following attack should receive Echoing Steel.
4. Exit Skyrim. Send BiggieTraitMechanics.log from:
   Documents/My Games/Skyrim Special Edition/SKSE/
   Copy it before restarting Skyrim, which replaces the log. Documents may
   be redirected to OneDrive on your PC. The first line must say 1.3.2.

The log records selected-trait status, the incoming normal shout event,
manual/Skald arming, animation phases, attack flags, bonus consumption,
expiry and the physical-hit damage values before/after this plugin's change.
A successful HIT line contains echo_applied=true and multiplier=1.50 for
one-handed/unarmed attacks, or multiplier=2.00 for two-handed weapons.
The numbers are engine HitData values at this hook, not final enemy HP loss
after all other mods, armor, difficulty, blocking and engine processing.
The hit notification confirms the multiplier was applied at this hook.

DIAGNOSTIC CAPTURE IS BOUNDED
The capture stops after 200 diagnostic records per save load. Reload a save
to capture again. It observes existing callbacks and adds no polling script,
timer or event listener. For normal play, open:
SKSE/Plugins/BiggieTraitMechanics.ini
Set Enabled=0 under [EchoingSteelDiagnostics], then restart Skyrim. To keep
the bounded log but hide notifications, leave Enabled=1 and set Notifications=0.

WHAT WAS CHECKED HERE
- Existing and expanded rules pass for 1.5x/2x damage, the five-second
  boundary, expiry, misses spending the bonus, repeated shouts refreshing
  without stacking, exclusions, trait removal and Skald's following attack.
- Combo transitions and repeated contacts preserve the intended next-attack
  behavior in the pure rules. Actual animation-mod callbacks need the log.
- Native Windows compilation and all three rule suites passed before release.
- ESP, all Papyrus scripts, thumbnails and other existing gameplay files are
  byte-identical to v2.6.1. Only the helper DLL changes; one INI is added.
- No new game records, masters, inventory items or save-format changes.
- No Skyrim process is available here; the live hit-path test is still pending.

----- Previous release documentation -----

'''


def read_zip(path):
    with ZipFile(path) as z:
        assert z.testzip() is None
        return {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}


def main():
    ap = argparse.ArgumentParser()
    for name in ('base', 'native', 'out'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    assert sha(args.base.read_bytes()) == BASE_SHA
    old = read_zip(args.base)
    manifest = json.loads(old['Validation.json'])
    assert manifest['version'] == '2.6.1'
    assert all(sha(old[n]) == digest for n, digest in manifest['files'].items())
    native = read_zip(args.native)
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['source_commit'] == SOURCE_COMMIT
    assert info['version'] == '1.3.2' and info['combined_version'] == '2.6.2'
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    dll = 'SKSE/Plugins/BiggieTraitMechanics.dll'
    ini = 'SKSE/Plugins/BiggieTraitMechanics.ini'
    assert native[dll][:2] == b'MZ' and sha(native[dll]) == info['dll_sha256']
    pe = struct.unpack_from('<I', native[dll], 0x3c)[0]
    assert native[dll][pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', native[dll], pe+4)[0] == 0x8664
    source = ROOT / 'three-traits/native'
    files = dict(old)
    for n, digest in info['source_sha256_lf'].items():
        data = (source / n).read_bytes().replace(b'\r\n', b'\n')
        assert sha(data) == digest, n
        files['Documentation/ThreeTraits/NativeSource/' + n] = data
    assert native[ini].replace(b'\r\n', b'\n') == (source / 'BiggieTraitMechanics.ini').read_bytes()
    files[dll], files[ini] = native[dll], native[ini]
    for n, data in native.items():
        if not n.startswith('SKSE/'):
            files['Documentation/ThreeTraits/NativeBuild/' + n] = data
    for n in ('README.md', 'tools/build_windows.ps1'):
        files['Documentation/ThreeTraits/NativeSource/' + n] = (source / n).read_bytes()
    docs = 'Documentation/EchoingSteelDiagnostics/'
    files[docs + 'README.txt'] = NOTES.encode()
    files[docs + 'package.py'] = Path(__file__).read_bytes()
    files['README.txt'] = NOTES.encode() + old['README.txt']
    files['Documentation/v2_6_1-Validation.json'] = old['Validation.json']
    gameplay = lambda n: not n.startswith('Documentation/') and n not in ('README.txt', 'Validation.json')
    changed = sorted(n for n in old if gameplay(n) and files[n] != old[n])
    added = sorted(n for n in files if gameplay(n) and n not in old)
    assert changed == [dll] and added == [ini], (changed, added)
    assert files['Biggie Traits - Combined.esp'] == old['Biggie Traits - Combined.esp']
    # Rule/casting implementation was deliberately not changed by diagnostics.
    for n in ('Rules.h', 'Skald.h', 'SkaldRuntime.h', 'LabVisit.h'):
        k = 'Documentation/ThreeTraits/NativeSource/src/' + n
        assert files[k] == old[k], n
    report = dict(version='2.6.2', purpose='bounded Echoing Steel runtime diagnosis',
        changed_gameplay_files=changed, added_gameplay_files=added, baseline_sha256=BASE_SHA,
        plugin_byte_identical=True, papyrus_byte_identical=True, rule_implementation_unchanged=True,
        capture_limit_per_load=200, diagnostic_capture_enabled=True,
        new_records=0, new_masters=0, in_game_tested=False, native_build=info)
    files[docs + 'Validation.json'] = js(report)
    manifest.update(version='2.6.2', baseline=args.base.name, baseline_sha256=BASE_SHA,
        validation=report, trait_mechanics_dll=info)
    manifest['files'] = {n: sha(b) for n, b in sorted(files.items()) if n != 'Validation.json'}
    files['Validation.json'] = js(manifest)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with ZipFile(args.out, 'w', ZIP_DEFLATED, compresslevel=9) as z:
        for n, b in sorted(files.items()): z.writestr(n, b)
    check = read_zip(args.out)
    assert all(sha(check[n]) == d for n, d in manifest['files'].items())
    assert sum(n.endswith('.esp') for n in check) == 1
    print(json.dumps(dict(path=str(args.out.resolve()), sha256=sha(args.out.read_bytes()),
        bytes=args.out.stat().st_size, changed_gameplay_files=changed, added_gameplay_files=added), indent=2))


if __name__ == '__main__':
    main()
