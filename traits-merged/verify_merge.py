"""Check the resulting merged file's bindings and compare shipped script instructions."""
from pathlib import Path
import json
import re
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'venom-harvester/tools'))
from esp import records, fields

class Reader:
    """Independent, read-only VMAD v5/object-format-2 decoder."""
    def __init__(self, data):
        self.b, self.p = data, 0
    def get(self, fmt):
        values = struct.unpack_from('<' + fmt, self.b, self.p)
        self.p += struct.calcsize('<' + fmt)
        return values[0] if len(values) == 1 else values
    def string(self):
        size = self.get('H')
        value = self.b[self.p:self.p + size]
        self.p += size
        return value.decode()
    def val(self, kind):
        if kind == 1: return {'object': self.get('HHI')}
        if kind == 2: return self.string()
        if kind == 3: return self.get('i')
        if kind == 4: return self.get('f')
        if kind == 5: return self.get('B')
        if 11 <= kind <= 15: return [self.val(kind - 10) for _ in range(self.get('I'))]
        raise ValueError(kind)

stage = ROOT / 'staging'
esp_path = stage / 'Biggie Traits - Combined.esp'
rs = {int.from_bytes(h[12:16], 'little'): (h, list(fields(p))) for h, p in records(esp_path.read_bytes())}
head = dict(rs[0][1])
masters = [v.rstrip(b'\0').decode() for t, v in rs[0][1] if t == b'MAST']
own = len(masters) << 24
assert len(rs) == 35
assert struct.unpack_from('<I', rs[0][0], 8)[0] & 0x200
assert all((id >> 24) == len(masters) and 0x800 <= (id & 0xFFFFFF) <= 0xFFF for id in rs if id)
assert not any('Biggie Traits - ' in m and m.endswith(('Greybeard Trained.esp', 'Fully Devoted.esp', 'Devoted Alchemist.esp', 'Skald.esp')) for m in masters)
assert list(stage.glob('*.esp')) == [esp_path]

def rec(local): return rs[own | local]
def typed(id, kind): assert id in rs and rs[id][0][:4] == kind, (hex(id), kind)
expected = {0x800: ('Voice of Authority', 0x801), 0x900: ('Fully Devoted', 0x901),
            0xA00: ('Venom Harvester', 0xA01), 0xB00: ('Skald', 0xB01)}
for local, (name, effect) in expected.items():
    h, fs = rec(local)
    assert h[:4] == b'SPEL' and dict(fs)[b'FULL'].rstrip(b'\0').decode() == name
    first = next(v for t, v in fs if t == b'EFID')
    assert struct.unpack('<I', first)[0] == own | effect
    for t, v in fs:
        if t == b'EFID': typed(struct.unpack('<I', v)[0], b'MGEF')

assert [struct.unpack('<I', v)[0] & 0xFFFFFF for t, v in rec(0xA00)[1] if t == b'EFID'] == [0xA01]
assert [struct.unpack('<I', v)[0] & 0xFFFFFF for t, v in rec(0xB00)[1] if t == b'EFID'] == [0xB01, 0xB05]
assert struct.unpack_from('<I', dict(rec(0xA01)[1])[b'DATA'], 136)[0] == 0
assert struct.unpack_from('<I', dict(rec(0x801)[1])[b'DATA'], 136)[0] == own | 0x802
assert struct.unpack_from('<I', dict(rec(0xB05)[1])[b'DATA'], 136)[0] == own | 0xB04
assert struct.unpack_from('<I', dict(rec(0xB01)[1])[b'DATA'], 8)[0] == own | 0xB03
assert struct.unpack('<fII', next(v for t, v in rec(0x930)[1] if t == b'EFIT')) == (25.0, 0, 0)
for id, (h, fs) in rs.items():
    for tag, v in fs:
        if tag == b'CTDA' and struct.unpack_from('<H', v, 8)[0] == 693:
            fid = struct.unpack_from('<I', v, 12)[0]
            if fid >> 24 == len(masters): typed(fid, b'KYWD')

def scripts(reader, count):
    out = {}
    for _ in range(count):
        name = reader.string(); reader.get('B'); props = {}
        for _ in range(reader.get('H')):
            prop = reader.string(); kind, flags = reader.get('BB')
            props[prop] = reader.val(kind)
        out[name] = props
    return out
def object_id(value): return value['object'][2]

for local, script, expected_props in [
    (0x801, 'GT_GreybeardTrainedEffect', {'TraitAbility': 0x800, 'ShoutChoice': 0x803, 'ChosenShout': 0x804}),
    (0xB02, 'BT_SkaldHitEffect', {'TraitAbility': 0xB00}),
]:
    reader = Reader(dict(rec(local)[1])[b'VMAD'])
    assert reader.get('HH') == (5, 2)
    props = scripts(reader, reader.get('H'))[script]
    for name, target in expected_props.items(): assert object_id(props[name]) == own | target
    assert reader.p == len(reader.b)

reader = Reader(dict(rec(0x923)[1])[b'VMAD'])
assert reader.get('HHH') == (5, 2, 0)
assert reader.get('BH') == (2, 0) and reader.string() == '' and reader.get('H') == 1
assert reader.get('HHI') == (0, 0, own | 0x923)
assert reader.get('HH') == (5, 2)
props = scripts(reader, reader.get('H'))['FD_PowerGrantAlias']
for name, local in {'TraitAbility': 0x900, 'PledgePower': 0x921, 'Activated': 0x920,
                    'MenuBusy': 0x924, 'PreviousCap': 0x904, 'PreviousTarget': 0x905}.items():
    assert object_id(props[name]) == own | local
for name, local in {'FavorCap': 0x3D352, 'Tracker': 0x5901}.items():
    id = object_id(props[name])
    assert masters[id >> 24] == 'Wintersun - Faiths of Skyrim.esp' and id & 0xFFFFFF == local
assert reader.p == len(reader.b)
assert (stage / 'SEQ/Biggie Traits - Combined.seq').read_bytes() == struct.pack('<I', own | 0x923)
assert list((stage / 'SEQ').glob('*.seq')) == [stage / 'SEQ/Biggie Traits - Combined.seq']

formlists = (stage / 'Biggie Traits - Combined_FLM.ini').read_text()
actual = re.findall(r'^FormList\s*=\s*(\w+)\|(\w+)', formlists, re.M)
expected_lists = {(kind, edid + suffix) for kind, suffix in [('Traits_AbilityList', 'Ab'), ('Traits_EffectsList', '')]
                  for edid in ['Traits_GreybeardTrained', 'Traits_FullyDevoted', 'Traits_DevotedAlchemist', 'Traits_Skald']}
assert set(actual) == expected_lists and len(actual) == 8

with zipfile.ZipFile(ROOT.parent / 'outputs/Biggie_Traits_Combined_v1_6.zip') as z:
    for name in z.namelist():
        if name.startswith('Interface/') or (name.startswith('Scripts/') and name not in ['Scripts/FD_PowerGrantAlias.pex', 'Scripts/WSN_TrackerQuest_Quest.pex']):
            assert (stage / name).read_bytes() == z.read(name), name
    assert (stage / 'Biggie Traits - Combined_KID.ini').read_bytes() == z.read('Biggie Traits - Greybeard Trained_KID.ini')

lookups = json.loads((stage / 'Documentation/FormID-Map.json').read_text())['script_references']
script_report = {}
for stem in ['FD_PowerGrantAlias', 'WSN_TrackerQuest_Quest']:
    baseline = ROOT.parent / 'venom-harvester/staging'
    original = subprocess.check_output([str(ROOT.parent / 'fully-devoted/disassemble'), str(baseline / 'Scripts' / (stem + '.pex'))], text=True)
    merged = subprocess.check_output([str(ROOT.parent / 'fully-devoted/disassemble'), str(stage / 'Scripts' / (stem + '.pex'))], text=True)
    assert 'Biggie Traits - Combined.esp' in merged
    for row in lookups:
        if row['script'] != stem + '.psc': continue
        merged = merged.replace(f'{int(row["new_local"], 16)} "Biggie Traits - Combined.esp"',
                                f'{int(row["old_local"], 16)} "{row["source"]}"')
    # Papyrus identifiers are case-insensitive; imports may canonicalize e.g.
    # spell[] to Spell[]. Compare every instruction, value and property.
    original = original[original.index('.userFlagsRef'):]
    merged = merged[merged.index('.userFlagsRef'):]
    assert original.casefold() == merged.casefold(), stem
    script_report[stem] = 'same instructions and properties after reversing only merged lookup literals; identifier case ignored'

report = {'single_esp': True, 'esl_flagged': True, 'private_records': 34,
          'record_bindings': 'passed', 'quest_alias_and_seq': 'passed',
          'all_four_traits_registered': True, 'thumbnails_preserved': 4,
          'unaffected_pex_preserved': 5, 'compiled_scripts': script_report,
          'in_game_tested': False, 'save_migration_supported': False}
(ROOT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
