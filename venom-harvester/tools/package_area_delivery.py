"""Package the checked area-spell ESP conversion and native 2.1.3 build."""
from pathlib import Path
import hashlib
import io
import json
import sys
import zipfile
from patch_area_records import patch


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
    assert info['version'] == '2.1.3' and info['runtime'] == '1.6.1170'
    assert info['windows_build'] == info['bleedout_log_tests'] == info['blast_target_tests'] == 'passed'
    assert info['native_test_suites'] == 12 and info['logged_bleedout_regression_cases'] == 3
    assert info['explosion_target_enumeration'] == 'ProcessLists::ForAllActors'
    assert info['explosion_uses_global_tes_sky_cell'] is False
    assert info['explosion_radius_and_space_rechecked'] is True
    assert info['explosion_target_handles_deduplicated'] is True
    assert info['explosion_area_delivery_tests'] == 'passed'
    assert info['explosion_health_change_logged'] is True
    assert info['explosion_acceptance_logged'] is True
    assert info['ordinator_dependency'] is False
    assert info['dll_sha256'] == sha(dll) and dll[:2] == b'MZ'
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous_info = json.loads(files['Documentation/CorpseExplosionTargetScanFix/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous_info['version'] == '2.1.2'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous_info['dll_sha256']
    prefix = 'Documentation/CorpseExplosionAreaDelivery/'
    files['Biggie Traits - Combined.esp'] = patch(files['Biggie Traits - Combined.esp'])
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    files[prefix + 'Native/BuildInfo.json'] = native['BuildInfo.json']
    files[prefix + 'Native/CommonLibSSE-LICENSE'] = native['CommonLibSSE-LICENSE']
    source = f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester'
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source, regression tests and packaging script:\n' + source + '\n'
        'Earlier source snapshots in this archive document previous releases.\n'
    ).encode()
    files[prefix + 'README.md'] = native['README.md']
    instructions = "BIGGIE TRAITS - COMBINED 2.13.3-beta1\nSkyrim Steam 1.6.1170 / SKSE\n\nCORPSE EXPLOSION: CORPSE-ORIGIN AREA SPELLS\nAdapts the delivery pattern examined in Ordinator Corpse Gas: the corpse\ncasts a real Self-area damage spell with its explosion attached to the magic\neffect. The player is credited. All four damage types, the 25% calculation,\nmatching resistances, the enemy-only 420-unit radius and no chains remain.\nNo Ordinator dependency or copied Ordinator assets are included.\n\nINSTALL\nExit Skyrim. Replace the previous Combined package in MO2 with this COMPLETE\narchive. Both Biggie Traits - Combined.esp and SKSE/Plugins/VenomHarvester.dll\nmust win conflicts. Keep the ESP enabled and restart through SKSE.\nThe DLL logs an error if an older ESP wins. No new game is required.\n\nRETEST\n1. Check VenomHarvester.log starts with version 2.1.3 and the ready line says\n   corpse-origin Self-area spells. There must be no area-record mismatch.\n2. Place two hostile enemies within about six metres and clear line of sight.\n   Let your weapon oil/poison kill one; check the survivor's Health.\n   Neutral animals and followers are excluded by the enemy-only rule.\n3. The log now separates area-cast requests, area-apply accepted=true/false,\n   and health-update actual-loss. Only the last confirms measured damage.\n4. If it still only shakes, send the fresh VenomHarvester.log. It will show\n   whether no enemy qualified, native application failed, or Health stayed\n   unchanged. Repeat with an elemental oil and check the Satchel refund.\n\nThe loaded-actor crash fix and bleedout/refund fixes are retained. Save\nformats and trait selection are unchanged. Only the native DLL, eight private\nCorpse Explosion spell/effect records and release documentation are updated.\nAll other ESP records, scripts, thumbnails and DLLs are preserved.\n\nTwelve automated native suites passed on Windows; ESP conversion and archive\npreservation checks passed. In-game damage/visual confirmation is still needed.\nSource/build hashes and validation are under Documentation/CorpseExplosionAreaDelivery.\n\nPREVIOUS RELEASE NOTES FOLLOW\n\n"
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll', 'Biggie Traits - Combined.esp'}, changed
    assert set(old) <= set(files)
    report = {
        'version': '2.13.3-beta1', 'baseline': '2.13.2-beta1',
        'source_commit': info['source_commit'], 'native_version': '2.1.3',
        'native_test_suites': 12, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed),
        'bleedout_regression_cases': 3,
        'loaded_actor_target_tests': 'passed',
        'global_tes_sky_cell_lookup_removed': True,
        'delivery': 'corpse-origin native Self-area spell',
        'changed_esp_records': 8, 'unchanged_trait_ids_and_save_records': True,
        'native_acceptance_and_actual_health_diagnostics': True,
        'ordinator_dependency_or_assets': False,
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
