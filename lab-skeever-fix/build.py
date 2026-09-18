"""Package the native Lab Skeever activation update into the existing combined mod."""
from pathlib import Path
import argparse, hashlib, json, zipfile, subprocess
ROOT = Path(__file__).resolve().parent.parent
HERE = Path(__file__).resolve().parent
sha = lambda b: hashlib.sha256(b).hexdigest()
jbytes = lambda obj: (json.dumps(obj, indent=2) + '\n').encode()
p = argparse.ArgumentParser()
p.add_argument('--native', type=Path, required=True)
p.add_argument('--commit', required=True)
a = p.parse_args()
base = ROOT/'outputs/Biggie_Traits_Combined_Single_ESP_v2_4.zip'
assert sha(base.read_bytes()) == '845764f7448565e7699d26a3233f1928cd3ac1f2f9b1c64f93b9e09224e1c307'
with zipfile.ZipFile(base) as z:
    assert z.testzip() is None
    original = {n:z.read(n) for n in z.namelist() if not n.endswith('/')}
files = original.copy()
with zipfile.ZipFile(a.native) as z:
    assert z.testzip() is None
    native = {n.replace('\\','/'):z.read(n) for n in z.namelist() if not n.endswith('/')}
info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
assert info['version'] == '1.1.0' and info['combined_version'] == '2.4.1'
assert info['source_commit'] == a.commit
assert info['windows_build'] == info['rules_tests'] == 'passed'
dll = 'SKSE/Plugins/BiggieTraitMechanics.dll'
assert sha(native[dll]) == info['dll_sha256']
for f,h in info['source_sha256_lf'].items():
    assert sha((ROOT/'three-traits/native'/f).read_text().encode()) == h, f
files[dll] = native[dll]
files['Scripts/BT_LabSkeeverEffect.pex'] = (HERE/'compiled/BT_LabSkeeverEffect.pex').read_bytes()
files['Source/Scripts/BT_LabSkeeverEffect.psc'] = (HERE/'papyrus/BT_LabSkeeverEffect.psc').read_bytes()
compiler = ROOT/'flail-v04/tools/compiler/papyrus-compiler/papyrus'
dump = subprocess.check_output([str(compiler),'read',str(HERE/'compiled/BT_LabSkeeverEffect.pex')], text=True)
assert 'ident(DispelSpell)' in dump and 'ident(Cast)' not in dump
handler = dump.split("name: 'OnGetUp'")[-1].split("name: 'OnEffectFinish'")[0]
assert "instructions count: '0'" in handler
files['Documentation/LabSkeever/ActivationFix/PEX-Inspection.txt'] = dump.encode()
files['Documentation/LabSkeever/ActivationFix/build.py'] = Path(__file__).read_bytes()
for f in ['src/main.cpp','src/Rules.h','src/LabVisit.h','tests/rules_tests.cpp','tests/lab_tests.cpp','CMakeLists.txt','vcpkg.json','tools/build_windows.ps1','README.md']:
    files['Documentation/ThreeTraits/NativeSource/'+f] = (ROOT/'three-traits/native'/f).read_bytes()
for f in ['BuildInfo.json','README.md','LICENSE','CommonLibSSE-LICENSE']:
    files['Documentation/ThreeTraits/NativeBuild/'+f] = native[f]
readme = '''Biggie Traits - Combined v2.4.1: Lab Skeever activation update

INSTALL
Close Skyrim. Install this complete archive in MO2 and replace the previous
combined package. Keep Biggie Traits - Combined.esp enabled. Launch with SKSE.
Do not install a second copy alongside the previous combined package.
Skyrim 1.6.1170 and the existing SKSE/Address Library requirements still apply.
No trait reselection is required for this activation change.

LAB SKEEVER
The SKSE helper now detects alchemy furniture and the alchemy crafting menu.
Leaving the session casts one refreshing 20-second bonus; duplicate exit events
do not stack it. The old Papyrus furniture handler is disabled. The existing
spell/perks still provide 30x beneficial-potion duration and 1.1x potency,
excluding food and poisons. The food-duration penalty remains 50%.

TEST
With Lab Skeever selected, use an alchemy lab, then leave it. Within 20 seconds,
drink a beneficial potion with a duration, such as a Resist or Fortify potion.
Check its resulting Active Effects duration, not only the inventory tooltip.
A 60-second potion should last about 1800 seconds before other modifiers.
The bonus window should appear as Lab Skeever - Fresh Inspiration.
A repeat lab visit refreshes the window. Other crafting stations should not.

DIAGNOSTICS
BiggieTraitMechanics.log in your SKSE log directory now reports Lab Skeever
initialization, detected alchemy visits and whether the bonus was active after
casting. If it still fails, send that log from the same game session and the
name of the potion used. Windows compilation and automated rules checks passed;
this package has not been tested inside Skyrim here.

All plugin records, trait thumbnails and other gameplay files are unchanged
from v2.4; the only gameplay replacements are the helper DLL and Lab script.

----- Previous combined-package documentation -----

'''
files['README.txt'] = readme.encode() + original['README.txt']
files['Documentation/v2_4-Validation.json'] = original['Validation.json']
changed_gameplay = sorted(n for n in original if not n.startswith('Documentation/') and n not in ['README.txt','Validation.json'] and files[n] != original[n])
assert changed_gameplay == sorted([dll,'Scripts/BT_LabSkeeverEffect.pex','Source/Scripts/BT_LabSkeeverEffect.psc'])
assert [n for n in files if n.lower().endswith('.esp')] == ['Biggie Traits - Combined.esp']
assert files['Biggie Traits - Combined.esp'] == original['Biggie Traits - Combined.esp']
thumbs = [n for n in original if n.startswith('Interface/TraitPics/')]
assert len(thumbs) == 10 and all(files[n] == original[n] for n in thumbs)
report = dict(version='2.4.1', plugin_byte_identical=True, unchanged_thumbnails=len(thumbs),
    new_records=0, new_masters=0, changed_gameplay_files=changed_gameplay,
    native_build=info, papyrus_cleanup_verified=True, in_game_tested=False,
    activation='Alchemy workbench entry/exit plus alchemy crafting menu; one grant per visit',
    bonus_seconds=20, duration_multiplier=30, magnitude_multiplier=1.1, food_duration_multiplier=0.5)
files['Documentation/LabSkeever/ActivationFix/Validation.json'] = jbytes(report)
manifest = json.loads(original['Validation.json'])
assert len(manifest['traits']) == 10
manifest.update(version='2.4.1', baseline=base.name, baseline_sha256=sha(base.read_bytes()),
    validation=report, trait_mechanics_dll=info, in_game_tested=False)
manifest['files'] = {n:sha(b) for n,b in sorted(files.items()) if n != 'Validation.json'}
files['Validation.json'] = jbytes(manifest)
out = ROOT/'outputs/Biggie_Traits_Combined_Single_ESP_v2_4_1.zip'
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for n,b in sorted(files.items()): z.writestr(n,b)
with zipfile.ZipFile(out) as z:
    assert z.testzip() is None
    for n,h in manifest['files'].items(): assert sha(z.read(n)) == h,n
print(json.dumps(dict(output=str(out),sha256=sha(out.read_bytes()),bytes=out.stat().st_size,validation=report),indent=2))
