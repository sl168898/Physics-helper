"""Package the checked 50% damage descriptions and native 2.1.5 build."""
from pathlib import Path
import hashlib
import io
import json
import math
import struct
import subprocess
import sys
import zipfile
from patch_damage50_records import patch
from patch_poison_nova_radius_records import RADIUS_FEET, RADIUS_UNITS, POISON_MODEL

BASELINE_ARCHIVE_SHA = 'f500108a510da89e45fca3e86b4a5599ee0fcd4d19179facd3499a2940b5e78a'
THUMBNAIL = 'Interface/TraitPics/Traits_CorpseExplosionAb.dds'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_zip(path):
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        names = [name for name in archive.namelist() if not name.endswith('/')]
        assert len(names) == len(set(names)), 'Duplicate ZIP paths'
        return {name: archive.read(name) for name in names}


def verify_dll_version(blob):
    u16 = lambda pos: struct.unpack_from('<H', blob, pos)[0]
    u32 = lambda pos: struct.unpack_from('<I', blob, pos)[0]
    assert blob[:2] == b'MZ'
    pe = u32(0x3c)
    assert blob[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x8664
    optional = pe+24
    assert u16(optional) == 0x20b
    sections = optional+u16(pe+20)

    def raw(rva):
        if rva < u32(optional+60):
            return rva
        for i in range(u16(pe+6)):
            section = sections+40*i
            va, size = u32(section+12), u32(section+16)
            if va <= rva < va+size:
                return u32(section+20)+rva-va
        raise ValueError(f'Unmapped RVA {rva:x}')

    def string(pos):
        return blob[pos:blob.index(b'\0', pos)].decode()

    export = raw(u32(optional+112))
    functions, names, ordinals = (raw(u32(export+offset)) for offset in (28, 32, 36))
    for i in range(u32(export+24)):
        if string(raw(u32(names+4*i))) == 'SKSEPlugin_Version':
            version = raw(u32(functions+4*u16(ordinals+2*i)))
            assert u32(version) == 1 and u32(version+4) == 0x02010050
            assert string(version+8) == 'VenomHarvester'
            return
    raise AssertionError('No SKSEPlugin_Version export')


def package(baseline, artifact, project, output):
    assert sha(baseline.read_bytes()) == BASELINE_ARCHIVE_SHA, 'Unexpected complete baseline'
    native = read_zip(artifact)
    if 'BuildInfo.json' not in native:
        archives = [value for name, value in native.items() if name.endswith('.zip')]
        assert len(archives) == 1
        native = read_zip(io.BytesIO(archives[0]))
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    dll = native['SKSE/Plugins/VenomHarvester.dll']
    assert info['version'] == '2.1.5' and info['runtime'] == '1.6.1170'
    assert info['combined_version'] == info['combined_package_required'] == '2.13.5-beta1'
    checks = ('windows_build', 'harvest_tests', 'menu_tests', 'crafting_tests',
        'inventory_event_tests', 'pending_crafts_tests', 'inventory_ownership_tests',
        'damage_observation_tests', 'corpse_explosion_tests', 'batch_name_tests',
        'bleedout_log_tests', 'blast_target_tests', 'explosion_area_delivery_tests',
        'corpse_explosion_radius_boundary_tests', 'corpse_explosion_overkill_and_previous_save_tests')
    assert all(info[name] == 'passed' for name in checks)
    assert info['native_test_suites'] == 12 and info['logged_bleedout_regression_cases'] == 3
    assert info['corpse_explosion_fraction'] == 0.50
    assert info['corpse_explosion_radius_feet'] == info['explosion_native_spell_area_feet'] == RADIUS_FEET
    assert math.isclose(info['corpse_explosion_radius_units'], RADIUS_UNITS, abs_tol=0.0001)
    assert info['corpse_explosion_preferred_poison_plugin'] == 'Requiem - Magic Redone.esp'
    assert info['corpse_explosion_preferred_poison_explosion_local_id'] == '005C32'
    assert info['corpse_explosion_poison_model_in_supplied_mod'] == POISON_MODEL[:-1].decode()
    assert info['corpse_explosion_preferred_visual_uses_winning_loaded_record'] is True
    assert info['explosion_target_enumeration'] == 'ProcessLists::ForAllActors'
    assert info['explosion_uses_global_tes_sky_cell'] is False
    assert info['explosion_radius_and_space_rechecked'] is True
    assert info['explosion_health_change_logged'] is info['explosion_acceptance_logged'] is True
    assert info['ordinator_dependency'] is info['in_game_tested'] is False
    assert info['dll_sha256'] == sha(dll)
    verify_dll_version(dll)
    assert subprocess.check_output(['git', '-C', str(project), 'rev-parse', 'HEAD'], text=True).strip() == info['source_commit']
    for name, expected in info['source_sha256_lf'].items():
        assert sha((project / name).read_text().encode()) == expected, name

    files = read_zip(baseline)
    old = dict(files)
    previous = json.loads(files['Documentation/CorpseExplosionPoisonNovaRadius/Native/BuildInfo.json'].decode('utf-8-sig'))
    assert previous['version'] == '2.1.4'
    assert sha(files['SKSE/Plugins/VenomHarvester.dll']) == previous['dll_sha256']
    files['Biggie Traits - Combined.esp'] = patch(files['Biggie Traits - Combined.esp'])
    files['SKSE/Plugins/VenomHarvester.dll'] = dll
    prefix = 'Documentation/CorpseExplosionDamage50/'
    for name in ('BuildInfo.json', 'CommonLibSSE-LICENSE', 'LICENSE'):
        files[prefix + 'Native/' + name] = native[name]
    source = f'https://github.com/sl168898/Physics-helper/tree/{info["source_commit"]}/venom-harvester'
    files[prefix + 'Native/Source-Location.txt'] = (
        'Current native source, regression tests and checked packaging scripts:\n' + source + '\n'
        'Earlier source snapshots in this archive document previous releases.\n').encode()
    files[prefix + 'README.md'] = native['README.md']
    instructions = r"""BIGGIE TRAITS - COMBINED 2.13.5-beta1
Skyrim Steam 1.6.1170 / SKSE

CORPSE EXPLOSION: DAMAGE INCREASED FROM 25% TO 50%
Corpse Explosion now deals 50% of the player's recorded actual Health damage
to the original enemy. The killing oil's damage types split this amount,
and each recipient's matching resistance still applies. A poison removing
200 remaining Health produces a 100-damage burst before resistance, even if
its nominal damage was 5,000. Both in-game descriptions now say 50%.

Poison bursts explicitly reuse Poison Nova's explosion from the loaded
Requiem - Magic Redone.esp. In the supplied version this is the
Noxcrab\Restoration\PoisonRuneExplosion.nif model, with its light and sounds.
Only presentation is reused. Magic Redone is already a Combined master;
no third-party assets or additional master are added to this archive.
Fire, frost and shock retain their elemental burst visuals.

All four damage boundaries and private explosion radii remain 25 feet,
7.62 metres (533 1/3 units), matching Ordinator Corpse Gas's outer area.
Its separate inner damage zone is not added. Matching resistance,
enemy-only filters and corpse line of sight,
no friendly fire/chains, the Health penalty and Satchel refunds are unchanged.
The 25-foot range and thumbnail are preserved.

INSTALL
Exit Skyrim. Replace the previous Combined mod in MO2 with this COMPLETE
archive. Both Biggie Traits - Combined.esp and SKSE/Plugins/VenomHarvester.dll
must win conflicts. Keep the ESP enabled and restart through SKSE.
No new game or trait reselection is required.

RETEST
1. VenomHarvester.log must begin with version 2.1.5; the ready line must say
   50 percent, radius 25 feet (533.333 units). Check poison visual source: Poison Nova
   and the next poison visual line for its loaded model path.
2. Use two hostile enemies within 7.6 metres and clear line of sight.
   Let your weapon oil/poison kill one; check the survivor's Health.
   A neutral deer, followers and your summons do not qualify as recipients.
3. Damage logs distinguish area-cast requests, area-apply acceptance and
   health-update actual-loss. Actual-loss is the measured damage evidence.
   If damage or visuals still fail, send a fresh VenomHarvester.log.
4. Also test an elemental oil and check Huntsman's Satchel refunds.

The previous crash/refund fixes, trait IDs and save formats are retained.
Only the native DLL, two description records
and release documentation change. Other assets, scripts and DLLs are preserved.
Twelve native automated suites passed on Windows; ESP and archive checks
passed. In-game visual and damage confirmation is still needed.
Source/build hashes and validation: Documentation/CorpseExplosionDamage50.

PREVIOUS RELEASE NOTES FOLLOW

"""
    files['README.txt'] = instructions.encode() + old['README.txt']
    changed = {name for name in old if files[name] != old[name]}
    assert changed == {'README.txt', 'SKSE/Plugins/VenomHarvester.dll', 'Biggie Traits - Combined.esp'}, changed
    assert set(old) <= set(files) and files[THUMBNAIL] == old[THUMBNAIL]
    report = {
        'version': '2.13.5-beta1', 'baseline': '2.13.4-beta1',
        'baseline_archive_sha256': BASELINE_ARCHIVE_SHA,
        'source_commit': info['source_commit'], 'native_version': '2.1.5',
        'native_test_suites': 12, 'windows_build': 'passed', 'in_game_tested': False,
        'dll_sha256': sha(dll), 'esp_sha256': sha(files['Biggie Traits - Combined.esp']),
        'skse_export_version_verified': '2.1.5.0',
        'changed_existing_files': sorted(changed),
        'preserved_existing_files': len(old) - len(changed),
        'changed_esp_records': 2, 'unchanged_trait_ids_masters_and_save_records': True,
        'damage_fraction': 0.50, 'overkill_and_previous_save_tests': 'passed',
        'radius_feet': RADIUS_FEET, 'radius_units': RADIUS_UNITS, 'radius_metres': 7.62,
        'radius_boundary_tests': 'passed', 'poison_visual_source_local_id': '005C32',
        'poison_visual_source_plugin': 'Requiem - Magic Redone.esp',
        'poison_model_in_supplied_mod': POISON_MODEL[:-1].decode(),
        'corpse_explosion_thumbnail_sha256': sha(files[THUMBNAIL]),
        'native_acceptance_and_actual_health_diagnostics': True,
        'ordinator_dependency_or_assets': False, 'third_party_assets_added': False,
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
