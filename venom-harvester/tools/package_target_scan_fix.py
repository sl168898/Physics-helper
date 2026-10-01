"""Patch the verified 2.13.1 Combined archive with a verified native 2.1.2 build."""
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
    assert info['version'] == '2.1.2' and info['runtime'] == '1.6.1170'
    assert info['windows_build'] == info['bleedout_log_tests'] == info['blast_target_tests'] == 'passed'
    assert info['native_test_suites'] == 11 and info['logged_bleedout_regression_cases'] == 3
    assert info['explosion_target_enumeration'] == 'ProcessLists::ForAllActors'
    assert info['explosion_uses_global_tes_sky_cell'] is False
    assert info['explosion_radius_and_space_rechecked'] is True
    assert info['explosion_target_handles_deduplicated'] is True
    assert info['dll_sha256'] == sha(dll) and dll[:2] == b'MZ'
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous_info = json.loads(files['Documentation/SatchelBleedoutFix/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous_info['version'] == '2.1.1'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous_info['dll_sha256']
    prefix = 'Documentation/CorpseExplosionTargetScanFix/'
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    files[prefix + 'Native/BuildInfo.json'] = native['BuildInfo.json']
    files[prefix + 'Native/CommonLibSSE-LICENSE'] = native['CommonLibSSE-LICENSE']
    source = f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester'
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source, regression tests and packaging script:\n' + source + '\n'
        'Earlier source snapshots in this archive document previous releases.\n'
    ).encode()
    files[prefix + 'README.md'] = native['README.md']
    instructions = "BIGGIE TRAITS - COMBINED 2.13.2-beta1\nSkyrim Steam 1.6.1170 / SKSE\n\nCORPSE EXPLOSION: FIX CRASH WHILE FINDING NEARBY ENEMIES\nThe supplied crash occurred after both traits recognized a poison killing\nblow. Corpse Explosion's nearby-reference search used an invalid world-space\npointer and crashed before the queued Satchel refund could run.\nVenomHarvester 2.1.2 replaces that search with a snapshot of loaded actor\nhandles, retaining the 420-unit 3D radius, same-space requirement, hostility,\nline of sight, teammate/summon exclusions and matching damage resistances.\nActors across exterior cell boundaries are included; separate interiors and\nworld spaces are excluded. Duplicate actors are only hit once per burst.\n\nINSTALL\nExit Skyrim. Replace the previous Combined package in MO2 with this complete\narchive. Its SKSE/Plugins/VenomHarvester.dll must win file conflicts.\nKeep Biggie Traits - Combined.esp enabled and restart through SKSE.\n\nRETEST\n1. Check VenomHarvester.log starts with version 2.1.2.\n2. Reload before the crash and use an unclaimed recorded poison batch.\n3. Apply it to a weak dagger and let poison kill the deer.\n4. Expect Poison lethal and Refunded ingredient. With Corpse Explosion\n   selected, also expect target scan and burst. These are separate queued\n   tasks, so their relative log order may vary.\n5. Test a nearby hostile target within about six metres and line of sight;\n   it should receive the matching damage type. Repeat indoors and near an\n   exterior cell boundary. If a crash remains, send both fresh logs.\n\nThe bleedout fix, recipes and once-per-batch refund accounting are retained.\nNo new game, trait reselection or save-format migration is required.\nThe DLL and release notes are updated; ESPs, scripts, thumbnails and all\nother DLLs are byte-identical to 2.13.1-beta1.\n\nAll eleven automated native suites passed on Windows. In-game confirmation\nis still needed. Source/build hashes and validation details are under\nDocumentation/CorpseExplosionTargetScanFix. The source README explains the\nexact failing call and the new actor-targeting regression coverage.\n\nPREVIOUS RELEASE NOTES FOLLOW\n\n"
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll'}, changed
    assert set(old) <= set(files)
    report = {
        'version': '2.13.2-beta1', 'baseline': '2.13.1-beta1',
        'source_commit': info['source_commit'], 'native_version': '2.1.2',
        'native_test_suites': 11, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed),
        'bleedout_regression_cases': 3,
        'loaded_actor_target_tests': 'passed',
        'global_tes_sky_cell_lookup_removed': True,
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
