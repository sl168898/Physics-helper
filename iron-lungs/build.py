"""Add Iron Lungs to the user's v2.6.3 combined package.

python build.py BASELINE.zip NATIVE_BUILD.zip OUTPUT.zip
python build.py BASELINE.zip --verify-plugin-only

NATIVE_BUILD.zip is the inner ZIP produced by the pinned Windows workflow.
The source directory is ../three-traits/native beside this script.
"""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile
from esp import records, fields, field, record

VERSION = '2.7.0-beta1'
PLUGIN = 'Biggie Traits - Combined.esp'
INI = 'Biggie Traits - Combined_FLM.ini'
SHA = lambda b: hashlib.sha256(b).hexdigest()
TEXT = lambda tag, value: field(tag, value.encode() + b'\0')
UINT = lambda tag, value: field(tag, struct.pack('<I', value))
ROOT = Path(__file__).resolve().parent
NATIVE = ROOT.parent / 'three-traits/native'
if not NATIVE.exists():
    NATIVE = ROOT.parents[1] / 'ThreeTraits/NativeSource'

README = '''BIGGIE TRAITS COMBINED v2.7.0-beta1 — IRON LUNGS

NEW TRAIT: IRON LUNGS
Manual Unrelenting Force has no new shout cooldown. It costs 25% of your
maximum Stamina, adjusted by your current shout-recovery multiplier, with a
minimum cost of 10% of maximum Stamina. It requires enough Stamina to pay.
The bonus is non-elemental MAGIC damage equal to 25% of your CURRENT Stamina
immediately before payment. Armor does not reduce this bonus; magic resistance
does. Normal shout-power magnitude perks, including Voice of Authority, apply.
The same formula applies to all three word levels; their existing base effects
are preserved. Hitting several enemies spends Stamina once, with one bonus
damage application per enemy. A miss still costs Stamina when the shout fires.

Cost = maximum Stamina * max(0.10, 0.25 * ShoutRecoveryMult)
Bonus before shout-power perks and resistance = current Stamina * 0.25
At 400 maximum/current Stamina: 100 cost and 100 base bonus.
At 400 maximum but 300 current: 100 cost and 75 base bonus.
With 40% cooldown reduction: 60 cost; the damage formula does not change.
Voice of Authority's +20% cooldown drawback also increases this cost.
The maximum includes equipment/temporary Fortify Stamina bonuses.

SKALD AND ECHOING STEEL
Skald's automatic first-word Unrelenting Force retains its normal effects,
its own 10/6/3-second recovery, and the power attack's ordinary Stamina cost.
It receives neither Iron Lungs bonus damage nor its additional Stamina charge.
Skald's existing NEXT-attack Echoing Steel interaction is preserved.
Manual Unrelenting Force still activates Echoing Steel normally.
Other shouts retain their usual costs, effects and recovery. Iron Lungs does
not erase an existing cooldown from a different shout.
No additional Stamina-regeneration penalty is added.

INSTALL / EXISTING PLAYTHROUGHS
1. Exit Skyrim. Replace the previous combined package in MO2 with this ZIP.
2. Keep the same Biggie Traits - Combined.esp enabled. Its ESP, FLM INI and
   SKSE/Plugins/BiggieTraitMechanics.dll must win file conflicts.
3. Load a save and select Iron Lungs through your normal trait menu.
   Existing traits retain their FormIDs; this update does not select it for you.
Requirements remain Skyrim Steam 1.6.1170, matching SKSE, Address Library,
original Biggie Traits, FormList Manipulator and the existing package masters.
There is ONE ESP and no additional master, inventory item or Papyrus script.

VALIDATION / BETA STATUS
Windows compilation, the four rule suites, ZIP/record checks, and preservation
of existing gameplay records/scripts/assets are verified before packaging.
This is an implementation beta, not an in-game-tested release. Native release
timing, projectile hit routing, resistance and interactions with your installed
casting/animation mods require gameplay testing. Transient projectile snapshots
are reset on load: a shout already in flight at save/reload loses its bonus.

SHORT IN-GAME CHECK
Use a separate test save. Select Iron Lungs and try one, two and three words.
- Below the actual Stamina cost: no shout effects; 'not enough Stamina' appears.
- Exactly enough: the shout works, spends the cost, and leaves no new cooldown.
- Repeat with lower current Stamina: the bonus decreases, the base cost does not.
- Hit several enemies: one payment, one bonus per enemy. Also try a missed shout.
- Try cooldown-reduction gear, Voice of Authority, and a magic-resistant enemy.
- Store Unrelenting Force with Skald: its power-attack cast has no new bonus/cost.
- Use a different shout and remove the trait: ordinary behavior is preserved.
- Test the manual shout followed by an Echoing Steel power attack.

DIAGNOSTICS
BiggieTraitMechanics.log in Documents/My Games/Skyrim Special Edition/SKSE
starts with version 1.4.0. 'Iron Lungs ready' confirms the hooks initialized.
[IronLungs] BLOCKED, CAST and HIT lines show gate decisions, payment/snapshot,
and bonus requests. HIT amounts are BEFORE resistance and other final damage
processing, not measured health loss. Logging is capped at 200 events per load.
If a check fails, send this log from the same test session before restarting.

Source and exact Windows-build provenance are included in Documentation.
Previous v2.6.3 Voice of Authority current-Speech/no-cap behavior is preserved.

PREVIOUS RELEASE NOTES (historical)

'''

def patch_plugin(original):
    previous = list(records(original))
    masters = [v for t, v in fields(previous[0][1]) if t == b'MAST']
    own = len(masters) << 24
    assert own == 0x21000000
    old = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in previous}
    ids = {'ability': own | 0xF30, 'marker': own | 0xF31,
           'damage_spell': own | 0xF32, 'damage_effect': own | 0xF33}
    assert not set(ids.values()) & set(old)
    marker = dict(fields(old[own | 0xF11][1]))[b'DATA']
    assert len(marker) == 152 and struct.unpack_from('<I', marker, 136)[0] == 0
    ability_spit = dict(fields(old[own | 0xF10][1]))[b'SPIT']
    description = ('Unrelenting Force has <no cooldown>, costs <25% of maximum Stamina> and deals extra '
                   '<magic damage equal to 25% of current Stamina>. Cooldown reduction lowers the cost, '
                   'to a <10% minimum>. Requires enough Stamina. Skald casts are exempt.')
    effect_description = ('Unrelenting Force: no cooldown; costs 25% maximum Stamina, adjusted by shout recovery '
                          '(minimum 10%); adds magic damage equal to 25% of current Stamina before payment. '
                          'Requires enough Stamina. Skald casts are exempt.')
    marker_record = (TEXT(b'EDID', 'Traits_IronLungs') + TEXT(b'FULL', 'Trait: Iron Lungs') +
                     UINT(b'MDOB', 0x435A5) + field(b'DATA', marker) + field(b'SNDD', b'') +
                     TEXT(b'DNAM', effect_description))
    ability = (TEXT(b'EDID', 'Traits_IronLungsAb') + field(b'OBND', bytes(12)) + TEXT(b'FULL', 'Iron Lungs') +
               UINT(b'ETYP', 0x13F44) + TEXT(b'DESC', description) + field(b'SPIT', ability_spit) +
               UINT(b'EFID', ids['marker']) + field(b'EFIT', struct.pack('<fII', 0, 0, 0)))
    # Serialized MGEF DATA (152 bytes), not the 64-bit runtime structure.
    damage_data = bytearray(152)
    struct.pack_into('<I', damage_data, 0, 0x8A05)  # hostile, detrimental, no duration/area, hidden
    struct.pack_into('<ii', damage_data, 12, -1, 44)  # no skill; ResistMagic
    struct.pack_into('<Ii', damage_data, 64, 0, 24)  # Value Modifier, Health
    struct.pack_into('<IIi', damage_data, 80, 1, 3, -1)  # fire-and-forget, target actor, no secondary AV
    struct.pack_into('<f', damage_data, 112, 1.0)
    struct.pack_into('<I', damage_data, 140, 2)
    damage_effect = (TEXT(b'EDID', 'BT_IronLungsMagicDamageEffect') + TEXT(b'FULL', 'Iron Lungs') +
                     field(b'DATA', damage_data) + field(b'SNDD', b'') +
                     TEXT(b'DNAM', 'Deals <mag> points of magic damage.'))
    # Cost zero, manual cost calculation, regular spell, fire-and-forget,
    # target actor. Neither Ignore Resistance nor No Absorb is set.
    damage_spit = struct.pack('<IIIIIIffI', 0, 1, 0, 0, 1, 3, 0, 0, 0)
    damage_spell = (TEXT(b'EDID', 'BT_IronLungsMagicDamage') + field(b'OBND', bytes(12)) +
                    TEXT(b'FULL', 'Iron Lungs') + TEXT(b'DESC', '') + field(b'SPIT', damage_spit) +
                    UINT(b'EFID', ids['damage_effect']) + field(b'EFIT', struct.pack('<fII', 1, 0, 0)))
    additions = {
        b'MGEF': record(b'MGEF', ids['marker'], marker_record) + record(b'MGEF', ids['damage_effect'], damage_effect),
        b'SPEL': record(b'SPEL', ids['ability'], ability) + record(b'SPEL', ids['damage_spell'], damage_spell)
    }
    out = bytearray()
    pos = 0
    inserted = set()
    while pos < len(original):
        header = bytearray(original[pos:pos + 24])
        size = struct.unpack_from('<I', header, 4)[0]
        if header[:4] == b'TES4':
            body = b''
            for tag, value in fields(original[pos + 24:pos + 24 + size]):
                if tag == b'HEDR':
                    fmt, count, next_id = struct.unpack('<fII', value)
                    value = struct.pack('<fII', fmt, count + 4, max(next_id, 0xF34))
                elif tag == b'SNAM': value = b'Eleven custom traits in one ESL-flagged ESP.\0'
                body += field(tag, value)
            struct.pack_into('<I', header, 4, len(body))
            out += header + body
            pos += 24 + size
        else:
            assert header[:4] == b'GRUP'
            body = original[pos + 24:pos + size]
            tag = bytes(header[8:12])
            if tag in additions:
                assert tag not in inserted and struct.unpack_from('<I', header, 12)[0] == 0
                body += additions[tag]; inserted.add(tag)
                struct.pack_into('<I', header, 4, len(body) + 24)
            out += header + body
            pos += size
    assert inserted == set(additions)
    after = list(records(bytes(out)))
    new = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in after}
    assert len(new) == len(old) + 4 == len(after)
    for fid, data in old.items():
        if fid: assert new[fid] == data, f'Existing record changed: {fid:08X}'
    assert [v for t, v in fields(after[0][1]) if t == b'MAST'] == masters
    assert after[0][0][8:12] == previous[0][0][8:12]  # ESL flags
    assert struct.unpack_from('<I', dict(fields(after[0][1]))[b'HEDR'], 4)[0] == len(after) - 1
    assert struct.unpack_from('<I', damage_data)[0] & (1 << 21) == 0
    assert struct.unpack_from('<i', damage_data, 16)[0] == 44
    assert struct.unpack_from('<II', damage_data, 64) == (0, 24)
    return bytes(out), {'new_records': {k: f'{v:08X}' for k, v in ids.items()},
                        'existing_records_byte_identical': len(old) - 1,
                        'new_masters': 0, 'esl_flag_preserved': True,
                        'bonus_damage': 'non-elemental magic; ResistMagic; native Health damage',
                        'double_magnitude_scaling_disabled': True}

def main():
    baseline = Path(sys.argv[1])
    with zipfile.ZipFile(baseline) as archive:
        assert archive.testzip() is None
        old = {n: archive.read(n) for n in archive.namelist() if not n.endswith('/')}
    files = dict(old)
    files[PLUGIN], report = patch_plugin(old[PLUGIN])
    if sys.argv[2] == '--verify-plugin-only':
        print(json.dumps(report, indent=2)); return
    with zipfile.ZipFile(sys.argv[2]) as archive:
        assert archive.testzip() is None
        native = {n.replace('\\', '/'): archive.read(n) for n in archive.namelist() if not n.endswith('/')}
    info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version'] == '1.4.0' and info['combined_version'] == VERSION
    assert info['windows_build'] == info['rules_tests'] == 'passed'
    dll_path = 'SKSE/Plugins/BiggieTraitMechanics.dll'
    assert native[dll_path][:2] == b'MZ' and SHA(native[dll_path]) == info['dll_sha256']
    for path, digest in info['source_sha256_lf'].items():
        assert SHA((NATIVE / path).read_bytes().replace(b'\r\n', b'\n')) == digest, path
    files[dll_path] = native[dll_path]
    for name, data in native.items():
        if not name.startswith('SKSE/'):
            files['Documentation/ThreeTraits/NativeBuild/' + name] = data
    for path in NATIVE.rglob('*'):
        if path.is_file() and '_build' not in path.parts:
            files['Documentation/ThreeTraits/NativeSource/' + path.relative_to(NATIVE).as_posix()] = path.read_bytes()
    files[INI] += b'\n; Iron Lungs: keep the selection and removal entries paired.\nFormList = Traits_AbilityList|Traits_IronLungsAb\nFormList = Traits_EffectsList|Traits_IronLungs\n'
    files['Interface/TraitPics/Traits_IronLungsAb.dds'] = old['Interface/TraitPics/Traits_GreybeardTrainedAb.dds']
    files['README.txt'] = README.encode() + old['README.txt']
    files['Documentation/IronLungs/v2_6_3-Baseline-Validation.json'] = old['Validation.json']
    for name in ['build.py', 'esp.py']:
        files['Documentation/IronLungs/Rebuild/' + name] = (ROOT / name).read_bytes()
    for path in ['tests/iron_lungs_tests.cpp', 'src/IronLungs.h', 'src/IronLungsRuntime.h']:
        assert path in info['source_sha256_lf']
    allowed = {PLUGIN, INI, dll_path, 'README.txt', 'Validation.json'}
    preserved = []
    for name, data in old.items():
        if name in allowed or name.startswith('Documentation/ThreeTraits/NativeBuild/') or name.startswith('Documentation/ThreeTraits/NativeSource/'):
            continue
        assert files[name] == data, name
        preserved.append(name)
    assert files[INI].count(b'FormList = Traits_AbilityList|') == 11
    assert files[INI].count(b'FormList = Traits_EffectsList|') == 11
    report.update(version=VERSION, native_build=info, in_game_tested=False,
                  local_rule_suites=4, preserved_existing_files=len(preserved),
                  papyrus_scripts_unchanged=True, new_inventory_items=0,
                  cost_max_stamina_fraction=0.25, minimum_cost_max_stamina_fraction=0.10,
                  bonus_current_stamina_fraction=0.25, snapshot_before_payment=True,
                  skald_additional_cost=False, skald_additional_damage=False,
                  unchanged_voice_of_authority=True, unchanged_echoing_steel=True)
    files['Documentation/IronLungs/Validation.json'] = json.dumps(report, indent=2).encode()
    previous_manifest = json.loads(old['Validation.json'])
    manifest = {'version': VERSION, 'baseline': baseline.name, 'baseline_sha256': SHA(baseline.read_bytes()),
                'traits': previous_manifest['traits'] + ['Iron Lungs'], 'validation': report,
                'trait_mechanics_dll': info,
                'files': {name: SHA(data) for name, data in sorted(files.items()) if name != 'Validation.json'}}
    files['Validation.json'] = json.dumps(manifest, indent=2).encode()
    output = Path(sys.argv[3])
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for name, data in sorted(files.items()): archive.writestr(name, data)
    with zipfile.ZipFile(output) as archive:
        assert archive.testzip() is None
        assert sum(n.endswith('.esp') for n in archive.namelist()) == 1
        for name, digest in manifest['files'].items(): assert SHA(archive.read(name)) == digest, name
    print(json.dumps({'output': str(output.resolve()), 'sha256': SHA(output.read_bytes()), 'bytes': output.stat().st_size,
                      'new_records': report['new_records'], 'existing_records_preserved': report['existing_records_byte_identical'],
                      'native_build': info['source_commit'], 'in_game_tested': False}, indent=2))

if __name__ == '__main__': main()
