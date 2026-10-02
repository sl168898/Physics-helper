"""Package native 2.1.7 and four checked native-area-LOS spell flag changes."""
from pathlib import Path
import io
import json
import subprocess
import sys
import zipfile
from package_damage50 import read_zip, sha, verify_dll_version, THUMBNAIL
from patch_native_los_records import patch

BASELINE_SHA = '55acc92c9a62810b14ebaf0a081a74d4a3d37aec845a609aa21b36df6bc72b52'


def package(baseline, artifact, project, output):
    assert sha(baseline.read_bytes()) == BASELINE_SHA
    native = read_zip(artifact)
    if 'BuildInfo.json' not in native:
        archives = [value for name, value in native.items() if name.endswith('.zip')]
        assert len(archives) == 1
        native = read_zip(io.BytesIO(archives[0]))
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    dll = native['SKSE/Plugins/VenomHarvester.dll']
    assert info['version'] == '2.1.7' and info['runtime'] == '1.6.1170'
    assert info['combined_version'] == info['combined_package_required'] == '2.13.7-beta1'
    checks = ('windows_build', 'harvest_tests', 'menu_tests', 'crafting_tests',
        'inventory_event_tests', 'pending_crafts_tests', 'inventory_ownership_tests',
        'damage_observation_tests', 'corpse_explosion_tests', 'batch_name_tests',
        'bleedout_log_tests', 'blast_target_tests', 'explosion_area_delivery_tests',
        'corpse_explosion_radius_boundary_tests', 'corpse_explosion_overkill_and_previous_save_tests')
    assert all(info[name] == 'passed' for name in checks)
    assert info['native_test_suites'] == 13
    assert info['actor_hook_tables'] == 'explicit RE::VTABLE_Actor/Character/PlayerCharacter arrays'
    assert info['actor_hook_table_bounds_checked_at_compile_time'] is True
    assert info['actor_health_hook_uses_actor_vtable'] is True
    assert info['hook_installation_diagnostics'] is True
    assert info['corpse_explosion_visibility'] == 'native area LOS; no Actor::HasLineOfSight recipient prefilter'
    assert info['corpse_explosion_spell_ignore_los'] is False
    assert info['corpse_explosion_logged_visibility_tests'] == 'passed'
    assert info['corpse_explosion_logged_visibility_cases'] == 7
    assert info['corpse_explosion_fraction'] == 0.50
    assert info['corpse_explosion_radius_feet'] == 25
    assert info['corpse_explosion_preferred_poison_explosion_local_id'] == '005C32'
    assert info['in_game_tested'] is False
    assert info['dll_sha256'] == sha(dll)
    verify_dll_version(dll, 0x02010070)
    assert subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip() == info['source_commit']
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous = json.loads(files['Documentation/CorpseExplosionStartupFix/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous['version'] == '2.1.6'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous['dll_sha256']
    files['Biggie Traits - Combined.esp'] = patch(files['Biggie Traits - Combined.esp'])
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    prefix = 'Documentation/CorpseExplosionNativeVisibility/'
    for name in ('BuildInfo.json', 'CommonLibSSE-LICENSE', 'LICENSE'):
        files[prefix + 'Native/' + name] = native[name]
    files[prefix + 'README.md'] = native['README.md']
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source and checked packaging scripts:\n'
        f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester\n'
        'Earlier source snapshots in this archive document previous releases.\n').encode()
    instructions = """BIGGIE TRAITS - COMBINED 2.13.7-beta1
Skyrim Steam 1.6.1170 / SKSE

DAMAGE FILTER FIX: USE NATIVE AREA VISIBILITY
The 2.1.6 log showed that every otherwise eligible nearby enemy was excluded
by our actor-to-corpse line-of-sight check. The engine reached our area hook,
but the empty allowed set rejected those hits before damage application.
The DLL now removes that actor-perception veto. Four private spell flags
enable native area LOS, matching the supplied Ordinator Corpse Gas setting.
Skyrim handles area visibility; our gate handles enemies, life, loaded state,
space, range, matching resistance and once-only damage per type.

INSTALL AND RETEST
1. Close Skyrim and replace the previous Combined mod with this COMPLETE
   archive. BOTH Biggie Traits - Combined.esp and VenomHarvester.dll must win
   conflicts in MO2. Launch through SKSE and load your existing save.
2. VenomHarvester.log must show native 2.1.7 and a ready line with
   'native area LOS'. A record mismatch means the correct ESP is not winning.
3. Put two hostile enemies close together in open space. Kill one with a
   poison/oil and check the survivor's Health. Retest an elemental oil too.
4. Look for 'eligible=true visibility=native-area', 'area-apply ... accepted=true'
   and 'health-update ... actual-loss'. Only actual-loss confirms Health damage.
   Send a fresh VenomHarvester.log if damage still fails.
5. Retest Huntsman's Satchel refunds with a fresh remembered crafting batch.

Includes the startup fix, 50% damage, 25-foot radius, Poison Nova visual,
thumbnail, enemy-only damage and matching resistance. No new game, save
cleaning or trait reselection is required. Save formats and form IDs are
preserved. Only the DLL, four spell flags and release documentation change.

Windows compilation and 13 automated suites passed, including seven logged
visibility-rejection cases. Their positions are synthetic in-range fixtures;
they test our filters, not Skyrim's collision or live Health modification.
Actual in-game damage and obstruction still need confirmation.
Details: Documentation/CorpseExplosionNativeVisibility.

PREVIOUS RELEASE NOTES FOLLOW

"""
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll', 'Biggie Traits - Combined.esp'}, changed
    assert set(old) <= set(files)
    assert files[THUMBNAIL] == old[THUMBNAIL]
    report = {
        'version': '2.13.7-beta1', 'baseline': '2.13.6-beta1',
        'baseline_archive_sha256': BASELINE_SHA,
        'source_commit': info['source_commit'], 'native_version': '2.1.7',
        'native_test_suites': 13, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'skse_export_version_verified': '2.1.7.0',
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed), 'changed_esp_records': 4,
        'actor_hook_tables_explicit_and_compile_time_bounds_checked': True,
        'actor_health_hook_no_longer_targets_tesobjectrefr': True,
        'unchanged_trait_ids_masters_and_save_records': True,
        'damage_fraction': 0.50, 'radius_feet': 25,
        'area_visibility': 'native spell LOS', 'spell_ignore_los': False,
        'actor_to_corpse_perception_prefilter': False,
        'logged_visibility_regression_cases': 7,
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
