"""Package native 2.1.6; preserve every ESP byte and existing gameplay asset."""
from pathlib import Path
import io
import json
import subprocess
import sys
import zipfile
from package_damage50 import read_zip, sha, verify_dll_version, THUMBNAIL

BASELINE_SHA = '2a1773b38b13dcdaa9b5876226d4626a8bbeab6739b5a1f9e40427082c453cc5'


def package(baseline, artifact, project, output):
    assert sha(baseline.read_bytes()) == BASELINE_SHA
    native = read_zip(artifact)
    if 'BuildInfo.json' not in native:
        archives = [value for name, value in native.items() if name.endswith('.zip')]
        assert len(archives) == 1
        native = read_zip(io.BytesIO(archives[0]))
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    dll = native['SKSE/Plugins/VenomHarvester.dll']
    assert info['version'] == '2.1.6' and info['runtime'] == '1.6.1170'
    assert info['combined_version'] == info['combined_package_required'] == '2.13.6-beta1'
    checks = ('windows_build', 'harvest_tests', 'menu_tests', 'crafting_tests',
        'inventory_event_tests', 'pending_crafts_tests', 'inventory_ownership_tests',
        'damage_observation_tests', 'corpse_explosion_tests', 'batch_name_tests',
        'bleedout_log_tests', 'blast_target_tests', 'explosion_area_delivery_tests',
        'corpse_explosion_radius_boundary_tests', 'corpse_explosion_overkill_and_previous_save_tests')
    assert all(info[name] == 'passed' for name in checks)
    assert info['native_test_suites'] == 12
    assert info['actor_hook_tables'] == 'explicit RE::VTABLE_Actor/Character/PlayerCharacter arrays'
    assert info['actor_hook_table_bounds_checked_at_compile_time'] is True
    assert info['actor_health_hook_uses_actor_vtable'] is True
    assert info['hook_installation_diagnostics'] is True
    assert info['corpse_explosion_fraction'] == 0.50
    assert info['corpse_explosion_radius_feet'] == 25
    assert info['corpse_explosion_preferred_poison_explosion_local_id'] == '005C32'
    assert info['in_game_tested'] is False
    assert info['dll_sha256'] == sha(dll)
    verify_dll_version(dll, 0x02010060)
    assert subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip() == info['source_commit']
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous = json.loads(files['Documentation/CorpseExplosionDamage50/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous['version'] == '2.1.5'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous['dll_sha256']
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    prefix = 'Documentation/CorpseExplosionStartupFix/'
    for name in ('BuildInfo.json', 'CommonLibSSE-LICENSE', 'LICENSE'):
        files[prefix + 'Native/' + name] = native[name]
    files[prefix + 'README.md'] = native['README.md']
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source and checked packaging scripts:\n'
        f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester\n'
        'Earlier source snapshots in this archive document previous releases.\n').encode()
    instructions = """BIGGIE TRAITS - COMBINED 2.13.6-beta1
Skyrim Steam 1.6.1170 / SKSE

STARTUP FIX: INVALID ADDRESS LIBRARY LOOKUP
Versions 2.13.3 through 2.13.5 could stop before the main menu with a
VenomHarvester.dll error about a huge missing Address Library ID.
An actor hook read beyond an inherited four-entry vtable address array.
The DLL now selects explicit actor arrays with compile-time bounds checks.
The Actor Health hook also uses the proper Actor table.
This was a DLL bug; reinstalling Address Library does not correct it.

INSTALL AND VERIFY
1. Close Skyrim. Replace the old Combined mod in MO2 with this COMPLETE
   archive. Ensure SKSE/Plugins/VenomHarvester.dll wins all file conflicts
   and Biggie Traits - Combined.esp stays enabled. Launch through SKSE.
2. First confirm that the main menu appears.
3. VenomHarvester.log must start with version 2.1.6. Look for five
   'installed ... hook' lines, CorpseExplosion ready, and Satchel Ready.
   If startup still fails, send the fresh log and exact error message.
4. Load your existing save. With two hostile enemies inside 7.6 metres
   and clear line of sight, kill one using poison/weapon oil. Check the
   survivor's Health and verify Huntsman's Satchel refunds.

The ESP is byte-for-byte unchanged from 2.13.5. All gameplay assets,
the 50% damage, 25-foot radius, Poison Nova visual, thumbnail, IDs and
save formats are preserved. No new game, save cleaning or trait reselection
is needed for this update. Only the DLL and release documentation change.

Windows compilation and 12 native test suites passed. Startup and gameplay
still require confirmation in the user's game. Technical details and hashes:
Documentation/CorpseExplosionStartupFix.

PREVIOUS RELEASE NOTES FOLLOW

"""
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll'}, changed
    assert set(old) <= set(files)
    assert files['Biggie Traits - Combined.esp'] == old['Biggie Traits - Combined.esp']
    assert files[THUMBNAIL] == old[THUMBNAIL]
    report = {
        'version': '2.13.6-beta1', 'baseline': '2.13.5-beta1',
        'baseline_archive_sha256': BASELINE_SHA,
        'source_commit': info['source_commit'], 'native_version': '2.1.6',
        'native_test_suites': 12, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'skse_export_version_verified': '2.1.6.0',
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed), 'changed_esp_records': 0,
        'actor_hook_tables_explicit_and_compile_time_bounds_checked': True,
        'actor_health_hook_no_longer_targets_tesobjectrefr': True,
        'unchanged_trait_ids_masters_and_save_records': True,
        'damage_fraction': 0.50, 'radius_feet': 25,
        'poison_visual_source_plugin': 'Requiem - Magic Redone.esp',
        'poison_visual_source_local_id': '005C32',
        'corpse_explosion_thumbnail_sha256': sha(files[THUMBNAIL]),
    }
    files[prefix + 'Package-Validation.json'] = (json.dumps(report, indent=2) + '\n').encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(files):
            archive.writestr(name, files[name])
    assert read_zip(output) == files
    report['archive_sha256'] = sha(output.read_bytes())
    report['archive_size'] = output.stat().st_size
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    package(*map(Path, sys.argv[1:5]))
