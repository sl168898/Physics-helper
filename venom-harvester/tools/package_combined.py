"""Validate the Windows build and assemble the existing combined mod upgrade."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / 'staging'
BASE = ROOT / 'base/extracted'
sha = lambda b: hashlib.sha256(b).hexdigest()
archive, commit, run = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    payload = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
assert set(payload) == {'SKSE/Plugins/VenomHarvester.dll', 'BuildInfo.json', 'README.md', 'LICENSE', 'CommonLibSSE-LICENSE'}
info = json.loads(payload['BuildInfo.json'].decode('utf-8-sig'))
assert info['source_commit'] == commit and info['runtime'] == '1.6.1170'
assert info['version'] == '1.0.0' and info['windows_build'] == 'passed' and info['harvest_tests'] == 'passed'
assert info['requires_combined_traits_v1_6'] and info['no_new_inventory_items'] and not info['in_game_tested']
for name, checksum in info['source_sha256_lf'].items():
    assert sha((ROOT / name).read_bytes().replace(b'\r\n', b'\n')) == checksum, name
dll = payload['SKSE/Plugins/VenomHarvester.dll']
assert sha(dll) == info['dll_sha256'] and dll[:2] == b'MZ'
pe = struct.unpack_from('<I', dll, 0x3c)[0]
assert dll[pe:pe + 4] == b'PE\0\0'
assert struct.unpack_from('<H', dll, pe + 4)[0] == 0x8664
assert struct.unpack_from('<H', dll, pe + 22)[0] & 0x2000
for symbol in [b'SKSEPlugin_Version', b'SKSEPlugin_Load', b'VenomHarvester', b'VH_Native',
               b'Recovered poison', b'Biggie Traits - Devoted Alchemist.esp',
               b'The White Phial - Tweaks and Enhancements.esp']:
    assert symbol in dll, symbol

docs = STAGE / 'Documentation/VenomHarvester'
docs.mkdir(parents=True, exist_ok=True)
old_docs = STAGE / 'Documentation/DevotedAlchemist'
if old_docs.exists(): shutil.rmtree(old_docs)
(STAGE / 'SKSE/Plugins').mkdir(parents=True, exist_ok=True)
(STAGE / 'SKSE/Plugins/VenomHarvester.dll').write_bytes(dll)
for name in ['LICENSE', 'CommonLibSSE-LICENSE']:
    (docs / name).write_bytes(payload[name])
info['source_url'] = f'https://github.com/sl168898/Physics-helper/tree/{commit}/venom-harvester'
info['windows_build_url'] = f'https://github.com/sl168898/Physics-helper/actions/runs/{run}'
(docs / 'BuildInfo.json').write_text(json.dumps(info, indent=2) + '\n')
(docs / 'README.txt').write_text((ROOT / 'README.md').read_text())
(docs / 'Artwork-Prompt.txt').write_text((ROOT / 'art/Artwork-Prompt.txt').read_text())
(STAGE / 'README.txt').write_text((ROOT / 'Combined-README.txt').read_text())

sys.path.insert(0, str(ROOT / 'tools'))
from esp import records, fields
esp = STAGE / 'Biggie Traits - Devoted Alchemist.esp'
recs = {int.from_bytes(h[12:16], 'little'): (h, p) for h, p in records(esp.read_bytes())}
assert set(recs) == {0, *range(0x07000800, 0x07000807)}
assert dict(fields(recs[0x07000800][1]))[b'FULL'] == b'Venom Harvester\0'
assert [v for t, v in fields(recs[0x07000800][1]) if t == b'EFID'] == [struct.pack('<I', 0x07000801)]
assert struct.unpack_from('<I', dict(fields(recs[0x07000801][1]))[b'DATA'], 136)[0] == 0
assert [v for t, v in fields(recs[0x07000802][1]) if t == b'EPFD'] == [struct.pack('<f', 1)]
assert int.from_bytes(recs[0][0][8:12], 'little') & 0x200

disassemble = str(ROOT.parent / 'fully-devoted/disassemble')
decompile = str(ROOT.parent / 'flail-v04/tools/decompile')
controller = subprocess.check_output([disassemble, str(STAGE / 'Scripts/FD_PowerGrantAlias.pex')], text=True)
native = subprocess.check_output([decompile, str(STAGE / 'Scripts/VH_Native.pex')], text=True)
assert 'Function Poll() Global Native' in native
for call in ['callstatic VH_Native Poll', 'callmethod MigrateVenomHarvester', 'callmethod DispelSpell',
             'callmethod RemovePerk', 'callmethod FDRemoveTrait']:
    assert call in controller, call
assert 'callmethod AddItem' not in controller

from PIL import Image
thumbnail = STAGE / 'Interface/TraitPics/Traits_DevotedAlchemistAb.dds'
assert Image.open(thumbnail).size == (1024, 704) and thumbnail.read_bytes()[84:88] == b'DXT5'

old = {str(p.relative_to(BASE)): p.read_bytes() for p in BASE.rglob('*') if p.is_file()}
new = {str(p.relative_to(STAGE)): p.read_bytes() for p in STAGE.rglob('*') if p.is_file()}
allowed = {'Biggie Traits - Devoted Alchemist.esp', 'Source/Scripts/FD_PowerGrantAlias.psc',
           'Scripts/FD_PowerGrantAlias.pex', 'Interface/TraitPics/Traits_DevotedAlchemistAb.dds',
           'README.txt', 'Validation.json'}
removed = set(old) - set(new)
assert removed == {n for n in old if n.startswith('Documentation/DevotedAlchemist/')}
changed = {n for n in old.keys() & new.keys() if old[n] != new[n]}
assert changed <= allowed, changed - allowed
for name in old:
    if name not in allowed and name not in removed: assert new[name] == old[name], name

report = {
    'trait': 'Venom Harvester', 'replaces': 'Devoted Alchemist', 'record_identity_preserved': True,
    'main_ability_local_id': '0x800', 'retained_dynamic_poison_form_list': '0x806',
    'old_crafting_perk_detached_and_neutralized': True, 'old_disease_effect_detached': True,
    'old_gifts_disabled': True, 'existing_selected_trait_migration': 'automatic one-time ability refresh',
    'native_magnitude_or_duration_multiplier': 0.75, 'poison_per_victim': 1,
    'return_item': 'original ordinary poison; captured selected liquid for White Phial',
    'thumbnail': {'size': [1024, 704], 'format': 'DXT5', 'visual_inspection': 'passed'},
    'windows_build': 'passed', 'native_rule_and_serialization_tests': 'passed',
    'compiled_papyrus_interface_and_migration': 'verified', 'in_game_tested': False,
    'unchanged_other_trait_esps_and_thumbnails': True,
    'unchanged_other_scripts_except_shared_controller': True,
    'source_commit': commit, 'windows_build_run': run,
}
(docs / 'Validation.json').write_text(json.dumps(report, indent=2) + '\n')
new = {str(p.relative_to(STAGE)): p.read_bytes() for p in STAGE.rglob('*') if p.is_file()}
manifest = {
    'version': '1.6', 'baseline': 'Biggie_Traits_Combined_v1_5.zip',
    'baseline_sha256': sha((ROOT / 'base/Biggie_Traits_Combined_v1_5.zip').read_bytes()),
    'traits': ['Voice of Authority', 'Fully Devoted', 'Venom Harvester', 'Skald'],
    'venom_harvester': report, 'in_game_tested': False,
    'changed_baseline_files': sorted(changed | {'Validation.json'}),
    'other_existing_files_preserved': sorted(set(old) - allowed - removed),
    'files': {n: sha(b) for n, b in sorted(new.items()) if n != 'Validation.json'},
}
(STAGE / 'Validation.json').write_text(json.dumps(manifest, indent=2) + '\n')
output = ROOT.parent / 'outputs/Biggie_Traits_Combined_v1_6.zip'
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for path in sorted(STAGE.rglob('*')):
        if path.is_file(): z.write(path, str(path.relative_to(STAGE)))
with zipfile.ZipFile(output) as z:
    assert z.testzip() is None
    for name, checksum in manifest['files'].items(): assert sha(z.read(name)) == checksum, name
result = {'file': str(output), 'size': output.stat().st_size, 'sha256': sha(output.read_bytes()),
          'dll_sha256': sha(dll), 'commit': commit, 'windows_run': run}
(ROOT / 'package-status.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
