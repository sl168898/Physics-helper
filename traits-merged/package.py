"""Validate the Windows DLL and package the fresh-save single-plugin edition."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
STAGE = ROOT / 'staging'
NATIVE = ROOT / 'native' if (ROOT / 'native').is_dir() else ROOT.parent / 'venom-harvester'
BASE = ROOT.parent / 'outputs/Biggie_Traits_Combined_v1_6.zip'
sha = lambda data: hashlib.sha256(data).hexdigest()
archive, native_commit, run, trait_commit = Path(sys.argv[1]), sys.argv[2], sys.argv[3], sys.argv[4]
assert sha(BASE.read_bytes()) == '1740adef87fa3e4a28fbdc2806f0ef0408f6d08e148b3f08b5c5289d060dcf57'
subprocess.run([sys.executable, str(ROOT / 'verify_merge.py')], check=True)
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    payload = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
assert set(payload) == {'SKSE/Plugins/VenomHarvester.dll', 'BuildInfo.json', 'README.md', 'LICENSE', 'CommonLibSSE-LICENSE'}
info = json.loads(payload['BuildInfo.json'].decode('utf-8-sig'))
assert info['source_commit'] == native_commit and info['runtime'] == '1.6.1170'
assert info['version'] == '1.1.0' and info['windows_build'] == 'passed' and info['harvest_tests'] == 'passed'
assert info['supports_legacy_and_merged_traits'] and info['no_new_inventory_items'] and not info['in_game_tested']
assert set(info['source_sha256_lf']) == {'src/main.cpp', 'src/Harvest.h', 'tests/harvest_tests.cpp', 'CMakeLists.txt', 'vcpkg.json'}
for name, checksum in info['source_sha256_lf'].items():
    assert sha((NATIVE / name).read_bytes().replace(b'\r\n', b'\n')) == checksum, name
dll = payload['SKSE/Plugins/VenomHarvester.dll']
assert sha(dll) == info['dll_sha256'] and dll[:2] == b'MZ'
pe = struct.unpack_from('<I', dll, 0x3c)[0]
assert dll[pe:pe + 4] == b'PE\0\0'
assert struct.unpack_from('<H', dll, pe + 4)[0] == 0x8664
assert struct.unpack_from('<H', dll, pe + 22)[0] & 0x2000
for symbol in [b'SKSEPlugin_Version', b'SKSEPlugin_Load', b'VenomHarvester', b'VH_Native',
               b'Biggie Traits - Combined.esp', b'Biggie Traits - Devoted Alchemist.esp',
               b'Trait layout: {}; plugin {}', b'The White Phial - Tweaks and Enhancements.esp']:
    assert symbol in dll, symbol

(STAGE / 'SKSE/Plugins').mkdir(parents=True, exist_ok=True)
(STAGE / 'SKSE/Plugins/VenomHarvester.dll').write_bytes(dll)
docs = STAGE / 'Documentation'
native_docs = docs / 'VenomHarvester'
native_docs.mkdir(parents=True, exist_ok=True)
for name in ['BuildInfo.json', 'LICENSE', 'CommonLibSSE-LICENSE']:
    (native_docs / name).write_bytes(payload[name])
(native_docs / 'Native-README.md').write_bytes(payload['README.md'])
for name, target in [('merge-report.json', 'Merge-Report.json'), ('validation.json', 'Reference-Validation.json'),
                     ('script-validation.json', 'Papyrus-Validation.json')]:
    shutil.copyfile(ROOT / name, docs / target)
shutil.copyfile(ROOT / 'README.txt', STAGE / 'README.txt')
with zipfile.ZipFile(BASE) as z:
    artwork_docs = [n for n in z.namelist() if n.startswith('Documentation/') and n.endswith('/Artwork-Prompt.txt')]
    assert len(artwork_docs) == 4
    for name in artwork_docs:
        target = STAGE / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(z.read(name))

files = {p.relative_to(STAGE).as_posix(): p.read_bytes() for p in STAGE.rglob('*') if p.is_file()}
by_ext = lambda ext: sorted(n for n in files if n.lower().endswith(ext))
assert by_ext('.esp') == ['Biggie Traits - Combined.esp']
assert by_ext('.seq') == ['SEQ/Biggie Traits - Combined.seq']
assert by_ext('.ini') == ['Biggie Traits - Combined_FLM.ini', 'Biggie Traits - Combined_KID.ini']
assert len(by_ext('.dds')) == 4 and len(by_ext('.pex')) == len(by_ext('.psc')) == 7
assert by_ext('.dll') == ['SKSE/Plugins/VenomHarvester.dll']
checks = json.loads((ROOT / 'validation.json').read_text())
manifest = {
    'version': '2.0', 'edition': 'single ESL-flagged ESP; fresh-save edition',
    'baseline': BASE.name, 'baseline_sha256': sha(BASE.read_bytes()),
    'traits': ['Voice of Authority', 'Fully Devoted', 'Venom Harvester', 'Skald'],
    'plugin': 'Biggie Traits - Combined.esp', 'checks': checks,
    'native_version': '1.1.0', 'windows_build': 'passed', 'native_tests': 'passed',
    'native_source_commit': native_commit, 'trait_source_commit': trait_commit,
    'source_url': f'https://github.com/sl168898/Physics-helper/tree/{trait_commit}/traits-merged',
    'windows_build_url': f'https://github.com/sl168898/Physics-helper/actions/runs/{run}',
    'windows_artifact_id': 10530988569,
    'windows_artifact_sha256': '25a85604df20e127b3782043a51500b8ffb8585a873a1f3dfee143917ec165ec',
    'save_migration_supported': False, 'in_game_tested': False,
    'files': {n: sha(data) for n, data in sorted(files.items()) if n != 'Validation.json'},
}
(STAGE / 'Validation.json').write_text(json.dumps(manifest, indent=2) + '\n')
output = ROOT.parent / 'outputs/Biggie_Traits_Combined_Single_ESP_v2_0.zip'
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in sorted(STAGE.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(STAGE).as_posix())
with zipfile.ZipFile(output) as z:
    assert z.testzip() is None
    assert len(z.namelist()) == len(set(z.namelist())) == len(manifest['files']) + 1
    for name, checksum in manifest['files'].items(): assert sha(z.read(name)) == checksum, name
result = {'file': str(output), 'size': output.stat().st_size, 'sha256': sha(output.read_bytes()),
          'dll_sha256': sha(dll), 'native_commit': native_commit, 'trait_commit': trait_commit, 'windows_run': run}
(ROOT / 'package-status.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
