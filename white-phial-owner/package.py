"""Package the verified v2.1 update; keep the native binary from v2.0 unchanged."""
from pathlib import Path
import hashlib
import json
import shutil
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
STAGE = ROOT / 'staging'
sha = lambda data: hashlib.sha256(data).hexdigest()
commit = sys.argv[1]
assert len(commit) == 40 and all(c in '0123456789abcdef' for c in commit)
baseline_path = ROOT / 'input/Biggie_Traits_Combined_Single_ESP_v2_0.zip'
with zipfile.ZipFile(baseline_path) as z:
    old_manifest = z.read('Validation.json')
    old_dll = z.read('SKSE/Plugins/VenomHarvester.dll')
assert (STAGE / 'SKSE/Plugins/VenomHarvester.dll').read_bytes() == old_dll
checks = json.loads((ROOT / 'validation.json').read_text())
assert checks['previous_records_identical'] == 34 and checks['all_five_traits_registered']
assert checks['compiled_papyrus_scripts'] == 4 and checks['in_game_tested'] is False
docs = STAGE / 'Documentation/WhitePhialOwner'
docs.mkdir(parents=True, exist_ok=True)
for src, dest in [('build-report.json', 'Build-Report.json'), ('validation.json', 'Validation.json'),
                  ('art/Artwork-Prompt.txt', 'Artwork-Prompt.txt'), ('BUILD.md', 'Build-Instructions.md')]:
    shutil.copyfile(ROOT / src, docs / dest)
shutil.copyfile(ROOT / 'README.txt', STAGE / 'README.txt')
(STAGE / 'Documentation/v2_0-Validation.json').write_bytes(old_manifest)
files = {p.relative_to(STAGE).as_posix(): p.read_bytes() for p in STAGE.rglob('*') if p.is_file()}
extension = lambda suffix: sorted(n for n in files if n.endswith(suffix))
assert extension('.esp') == ['Biggie Traits - Combined.esp']
assert extension('.seq') == ['SEQ/Biggie Traits - Combined.seq']
assert len(extension('.dds')) == 5 and len(extension('.pex')) == len(extension('.psc')) == 11
assert extension('.dll') == ['SKSE/Plugins/VenomHarvester.dll']
manifest = {
    'version': '2.1', 'plugin': 'Biggie Traits - Combined.esp', 'esl_flagged': True,
    'traits': ['Voice of Authority', 'Fully Devoted', 'Venom Harvester', 'Skald', 'The White Phial Owner'],
    'baseline': baseline_path.name, 'baseline_sha256': sha(baseline_path.read_bytes()),
    'source_commit': commit,
    'source_url': f'https://github.com/sl168898/Physics-helper/tree/{commit}/white-phial-owner',
    'native_dll': {'version': '1.1.0', 'unchanged_from_v2_0': True, 'sha256': sha(old_dll)},
    'validation': checks, 'in_game_tested': False,
    'four_esp_save_migration_supported': False,
    'files': {n: sha(data) for n, data in sorted(files.items()) if n != 'Validation.json'},
}
(STAGE / 'Validation.json').write_text(json.dumps(manifest, indent=2) + '\n')
output = ROOT.parent / 'outputs/Biggie_Traits_Combined_Single_ESP_v2_1.zip'
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for p in sorted(STAGE.rglob('*')):
        if p.is_file(): z.write(p, p.relative_to(STAGE).as_posix())
with zipfile.ZipFile(output) as z:
    assert z.testzip() is None
    assert len(z.namelist()) == len(set(z.namelist())) == len(manifest['files']) + 1
    for name, checksum in manifest['files'].items(): assert sha(z.read(name)) == checksum, name
result = {'file': str(output), 'size': output.stat().st_size, 'sha256': sha(output.read_bytes()),
          'source_commit': commit, 'new_trait': 'The White Phial Owner', 'plugin_count': 1,
          'trait_count': 5, 'in_game_tested': False}
(ROOT / 'package-status.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result, indent=2))
