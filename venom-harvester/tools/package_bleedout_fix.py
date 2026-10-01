"""Patch the verified 2.13.0 Combined archive with a verified native 2.1.1 build."""
from pathlib import Path
import hashlib
import io
import json
import sys
import zipfile


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_zip(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        return {name: archive.read(name) for name in archive.namelist() if not name.endswith('/')}


def package(baseline, artifact, project, output):
    native = read_zip(artifact)
    if 'BuildInfo.json' not in native:
        archives = [value for name, value in native.items() if name.endswith('.zip')]
        assert len(archives) == 1
        native = read_zip(io.BytesIO(archives[0]))
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    dll = native['SKSE/Plugins/VenomHarvester.dll']
    assert info['version'] == '2.1.1' and info['runtime'] == '1.6.1170'
    assert info['windows_build'] == info['bleedout_log_tests'] == 'passed'
    assert info['native_test_suites'] == 10 and info['logged_bleedout_regression_cases'] == 3
    assert info['dll_sha256'] == sha(dll) and dll[:2] == b'MZ'
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous_info = json.loads(files['Documentation/CorpseExplosion/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous_info['version'] == '2.1.0'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous_info['dll_sha256']
    prefix = 'Documentation/SatchelBleedoutFix/'
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    files[prefix + 'Native/BuildInfo.json'] = native['BuildInfo.json']
    files[prefix + 'Native/CommonLibSSE-LICENSE'] = native['CommonLibSSE-LICENSE']
    source = f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester'
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source, regression tests and packaging script:\n' + source + '\n'
        'Earlier source snapshots in this archive document previous releases.\n'
    ).encode()
    files[prefix + 'README.md'] = native['README.md']
    instructions = '''BIGGIE TRAITS - COMBINED 2.13.1-beta1
Skyrim Steam 1.6.1170 / SKSE

HUNTSMAN'S SATCHEL: MISSED POISON KILLS AFTER BLEEDOUT
The supplied dagger-kill log showed a correctly tracked poison reducing a
victim from 22.4754 Health to 2.0945, entering bleedout, then to -18.2863 and
queuing death. The detector incorrectly stopped observing when bleedout began.
VenomHarvester 2.1.1 keeps observing nonessential actors with positive Health
in that state. Both Satchel refunds and Corpse Explosion now use this rule.
Bleedout alone is not a kill. Actual lethal Health damage and newly queued or
actual death are still required; essential bleedout and duplicate payouts are
still excluded. The final damage tally excludes overkill.

INSTALL
Exit Skyrim. Replace the previous Combined package in MO2 with this complete
archive and let SKSE/Plugins/VenomHarvester.dll win file conflicts. Keep the
same Biggie Traits - Combined.esp enabled. Restart through SKSE. No new game,
trait reselection, ESP change or save-format migration is required.

RETEST
1. Check VenomHarvester.log begins with version 2.1.1.
2. Reload a save before the failed kill, or prepare a fresh unclaimed batch.
3. Use the recorded Damage Health poison on a weak, unenchanted dagger. Let
   poison deliver the killing blow and check the original ingredient counts.
4. Expect Poison lethal, followed by Refunded ingredient. If Corpse Explosion
   is selected, expect its confirmed killing blow and a single burst as well.
5. If it fails, copy VenomHarvester.log before restarting Skyrim. It now logs
   real pre-tick Health, state-before, essential status and rejection reasons.

Already-missed kills cannot be refunded retroactively: the old detector never
recorded a payable kill. Existing recipes and unclaimed batches remain usable.
Coated-ammunition proxy identity is a separate compatibility check; this patch
addresses the confirmed dagger-test failure, not that separate issue.

All ten automated native suites passed on Windows. The regression includes
all three failed Health sequences from the supplied log. This package still
requires an in-game retest. Build hashes and source reference are under
Documentation/SatchelBleedoutFix. All ESPs, scripts, thumbnails and other DLLs
are byte-identical to 2.13.0-beta1.

PREVIOUS RELEASE NOTES FOLLOW

'''
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll'}, changed
    assert set(old) <= set(files)
    report = {
        'version': '2.13.1-beta1', 'baseline': '2.13.0-beta1',
        'source_commit': info['source_commit'], 'native_version': '2.1.1',
        'native_test_suites': 10, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed),
        'bleedout_regression_cases': 3,
    }
    files[prefix + 'Package-Validation.json'] = (json.dumps(report, indent=2) + '\n').encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(files):
            archive.writestr(name, files[name])
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None and len(archive.namelist()) == len(files)
        assert archive.read('SKSE/Plugins/VenomHarvester.dll') == dll
    report['archive_sha256'] = sha(output.read_bytes())
    report['archive_size'] = output.stat().st_size
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    package(*map(Path, sys.argv[1:5]))
