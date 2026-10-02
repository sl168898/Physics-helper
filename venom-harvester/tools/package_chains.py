"""Package native 2.2.0 plus two descriptions, preserving the complete trait mod."""
from pathlib import Path
import io
import json
import subprocess
import sys
import zipfile
from package_damage50 import read_zip, sha, verify_dll_version, THUMBNAIL
from patch_chain_records import patch

BASELINE_SHA = '23aa51d5ea3e46b3d2486a204c487069177c82c3983d52fa8214c8649342385e'


def package(baseline, artifact, project, output):
    assert sha(baseline.read_bytes()) == BASELINE_SHA
    native = read_zip(artifact)
    if 'BuildInfo.json' not in native:
        archives = [value for name, value in native.items() if name.endswith('.zip')]
        assert len(archives) == 1
        native = read_zip(io.BytesIO(archives[0]))
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    dll = native['SKSE/Plugins/VenomHarvester.dll']
    assert info['version'] == '2.2.0' and info['runtime'] == '1.6.1170'
    assert info['combined_version'] == info['combined_package_required'] == '2.14.0-beta1'
    checks = ('windows_build', 'harvest_tests', 'menu_tests', 'crafting_tests',
        'inventory_event_tests', 'pending_crafts_tests', 'inventory_ownership_tests',
        'damage_observation_tests', 'corpse_explosion_tests', 'batch_name_tests',
        'bleedout_log_tests', 'blast_target_tests', 'explosion_area_delivery_tests',
        'corpse_explosion_radius_boundary_tests', 'corpse_explosion_overkill_and_previous_save_tests',
        'corpse_explosion_chain_tests', 'corpse_explosion_production_pump_tests')
    assert all(info[name] == 'passed' for name in checks)
    assert info['native_test_suites'] == 15
    assert info['actor_hook_tables'] == 'explicit RE::VTABLE_Actor/Character/PlayerCharacter arrays'
    assert info['actor_hook_table_bounds_checked_at_compile_time'] is True
    assert info['actor_health_hook_uses_actor_vtable'] is True
    assert info['corpse_explosion_visibility'] == 'native area LOS; no Actor::HasLineOfSight recipient prefilter'
    assert info['corpse_explosion_spell_ignore_los'] is False
    assert info['corpse_explosion_fraction'] == 0.50 and info['corpse_explosion_flat_damage'] == 100
    assert info['corpse_explosion_chains'] and info['corpse_explosion_flat_bonus_per_corpse']
    assert info['corpse_explosion_nonrecursive_waves'] and info['corpse_explosion_max_bursts_per_wave'] == 16
    assert info['corpse_explosion_radius_feet'] == 25
    assert info['corpse_explosion_preferred_poison_explosion_local_id'] == '005C32'
    assert info['corpse_explosion_save_record'] == 'CEXP v2 (reads and migrates v1)'
    assert info['in_game_tested'] is False and info['dll_sha256'] == sha(dll)
    verify_dll_version(dll, 0x02020000)
    assert subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip() == info['source_commit']
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline); old = dict(files)
    previous = json.loads(files['Documentation/CorpseExplosionNativeVisibility/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous['version'] == '2.1.7' and sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous['dll_sha256']
    files['Biggie Traits - Combined.esp'] = patch(files['Biggie Traits - Combined.esp'])
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    prefix = 'Documentation/CorpseExplosionChains/'
    for name in ('BuildInfo.json', 'CommonLibSSE-LICENSE', 'LICENSE'):
        files[prefix + 'Native/' + name] = native[name]
    files[prefix + 'README.md'] = native['README.md']
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source, tests and checked packaging scripts:\n'
        f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester\n'
        'Earlier source snapshots in this archive document previous releases.\n').encode()
    instructions = native['README.md'].decode('utf-8-sig').split('# Version 2.1.7 beta:')[0]
    files['README.txt'] = ('BIGGIE TRAITS - COMBINED 2.14.0-beta1\n\n' + instructions +
        '\nPREVIOUS RELEASE NOTES FOLLOW\n\n').encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll', 'Biggie Traits - Combined.esp'}, changed
    assert set(old) <= set(files) and files[THUMBNAIL] == old[THUMBNAIL]
    report = {
        'version': '2.14.0-beta1', 'baseline': '2.13.7-beta1',
        'baseline_archive_sha256': BASELINE_SHA,
        'source_commit': info['source_commit'], 'native_version': '2.2.0',
        'native_test_suites': 15, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'skse_export_version_verified': '2.2.0.0',
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed), 'changed_esp_records': 2,
        'unchanged_trait_ids_masters_and_satchel_save_records': True,
        'corpse_save_record': 'CEXP v2; reads v1 and upgrades pending bursts',
        'damage_fraction': 0.50, 'flat_damage': 100, 'flat_bonus_per_corpse': True,
        'chain_reactions': True, 'max_bursts_per_wave': 16, 'recursive_casting': False,
        'radius_feet': 25, 'area_visibility': 'native spell LOS',
        'actor_hook_tables_explicit_and_compile_time_bounds_checked': True,
        'poison_visual_source_plugin': 'Requiem - Magic Redone.esp',
        'poison_visual_source_local_id': '005C32',
        'corpse_explosion_thumbnail_sha256': sha(files[THUMBNAIL]),
    }
    files[prefix + 'Package-Validation.json'] = (json.dumps(report, indent=2) + '\n').encode()
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name in sorted(files): archive.writestr(name, files[name])
    assert read_zip(output) == files
    report['archive_sha256'] = sha(output.read_bytes())
    report['archive_size'] = output.stat().st_size
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    package(*map(Path, sys.argv[1:5]))
