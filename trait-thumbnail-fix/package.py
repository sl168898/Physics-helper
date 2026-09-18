"""Publish the three-file overlay and the corrected full v2.1.1 bundle."""
from pathlib import Path
import hashlib
import json
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
sha = lambda b: hashlib.sha256(b).hexdigest()
commit = sys.argv[1]
assert len(commit) == 40 and all(c in '0123456789abcdef' for c in commit)
report = json.loads((ROOT / 'validation.json').read_text())
report['source_commit'] = commit
report['source_url'] = f'https://github.com/sl168898/Physics-helper/tree/{commit}/trait-thumbnail-fix'
with zipfile.ZipFile(ROOT / 'input' / report['baseline']) as z:
    base = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
fixed = {}
for entry in report['textures']:
    name = entry['file']
    data = (ROOT / 'overlay' / name).read_bytes()
    assert sha(data) == entry['new_sha256'] and sha(base[name]) == entry['old_sha256']
    assert data[128:] == base[name][128:]
    fixed[name] = data
assert len(fixed) == 3
readme = (ROOT / 'README.txt').read_bytes()
validation = (json.dumps(report, indent=2) + '\n').encode()
overlay = dict(fixed)
overlay['README-Thumbnail-Fix.txt'] = readme
overlay['Documentation/Thumbnail-Header-Fix.json'] = validation

bundle = dict(base)
bundle.update(fixed)
bundle['Documentation/Thumbnail-Header-Fix.json'] = validation
bundle['Documentation/Thumbnail-Fix-README.txt'] = readme
bundle['Documentation/v2_1-Validation.json'] = base['Validation.json']
intro = ('BIGGIE TRAITS COMBINED v2.1.1 -- THUMBNAIL HEADER FIX\n\n'
         'Corrects the malformed DDS headers for Skald, Venom Harvester and\n'
         'Voice of Authority. Their artwork is unchanged. All ESP, script, DLL,\n'
         'INI and SEQ files are byte-identical to v2.1. Replace your v2.1 package\n'
         'with this ZIP in MO2, or use the separate three-texture overlay.\n'
         'Restart Skyrim completely after installation. No new game or trait\n'
         'reselection is needed when updating v2.1. The older four-ESP merge\n'
         'restriction still applies if installing this complete bundle.\n\n'
         'The v2.1 gameplay and installation notes follow.\n\n').encode()
bundle['README.txt'] = intro + base['README.txt']
for name, data in base.items():
    if name not in fixed and name not in ['README.txt', 'Validation.json']:
        assert bundle[name] == data, name
runtime = [n for n in base if n.endswith(('.esp', '.pex', '.psc', '.dll', '.seq', '.ini'))]
assert all(bundle[n] == base[n] for n in runtime)
old_manifest = json.loads(base['Validation.json'])
manifest = dict(old_manifest)
manifest.update(version='2.1.1', source_commit=commit,
    source_url=report['source_url'], baseline=report['baseline'], baseline_sha256=report['baseline_sha256'],
    thumbnail_header_fix=report, runtime_files_identical_to_v2_1=runtime,
    files={n: sha(data) for n, data in sorted(bundle.items()) if n != 'Validation.json'})
bundle['Validation.json'] = (json.dumps(manifest, indent=2) + '\n').encode()

out = ROOT.parent / 'outputs'
results = []
for filename, files in [('Biggie_Traits_Thumbnail_Fix_v1.zip', overlay),
                        ('Biggie_Traits_Combined_Single_ESP_v2_1_1.zip', bundle)]:
    target = out / filename
    with zipfile.ZipFile(target, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name, data in sorted(files.items()): z.writestr(name, data)
    with zipfile.ZipFile(target) as z:
        assert z.testzip() is None and len(z.namelist()) == len(set(z.namelist())) == len(files)
        assert {n: z.read(n) for n in z.namelist()} == files
    results.append({'file': str(target), 'size': target.stat().st_size, 'sha256': sha(target.read_bytes())})
(ROOT / 'package-status.json').write_text(json.dumps(results, indent=2) + '\n')
print(json.dumps(results, indent=2))
