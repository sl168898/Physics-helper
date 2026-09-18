"""Independent checks of the appended trait, bindings, compiled calls and assets."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'venom-harvester/tools'))
from esp import records, fields
STAGE = ROOT / 'staging'
NAME = 'Biggie Traits - Combined.esp'
with zipfile.ZipFile(ROOT / 'input/Biggie_Traits_Combined_Single_ESP_v2_0.zip') as z:
    baseline = {n: z.read(n) for n in z.namelist()}
def parsed(raw): return {struct.unpack_from('<I', h, 12)[0]: (h, list(fields(b))) for h, b in records(raw)}
old = parsed(baseline[NAME])
new = parsed((STAGE / NAME).read_bytes())
masters = [v for t, v in new[0][1] if t == b'MAST']
assert masters == [v for t, v in old[0][1] if t == b'MAST']
own = len(masters) << 24
phial_master = masters.index(b'The White Phial - Tweaks and Enhancements.esp\0') << 24
assert len(new) == 41
assert set(new) - set(old) == {own | n for n in range(0xC00, 0xC06)}
for fid, value in old.items():
    if fid: assert new[fid] == value, hex(fid)
assert struct.unpack_from('<I', new[0][0], 8)[0] & 0x200
assert struct.unpack('<fII', dict(new[0][1])[b'HEDR'])[1:] == (40, 0xC06)
assert all((fid >> 24) == len(masters) and 0x800 <= (fid & 0xFFFFFF) < 0x1000 for fid in new if fid)
edids = [dict(fs)[b'EDID'] for fid, (h, fs) in new.items() if fid]
assert len(set(edids)) == len(edids)
def rec(n): return new[own | n]
def values(n, tag): return [v for t, v in rec(n)[1] if t == tag]
assert rec(0xC00)[0][:4] == b'SPEL'
assert values(0xC00, b'FULL') == [b'The White Phial Owner\0']
assert values(0xC00, b'EFID') == [struct.pack('<I', own | 0xC01)]
assert values(0xC00, b'SPIT') == values(0x800, b'SPIT')
assert rec(0xC01)[0][:4] == b'MGEF'
data = values(0xC01, b'DATA')[0]
assert struct.unpack_from('<I', data, 136)[0] == own | 0xC02
assert data[:136] == values(0x801, b'DATA')[0][:136] and data[140:] == values(0x801, b'DATA')[0][140:]
assert rec(0xC02)[0][:4] == b'PERK'
assert values(0xC02, b'DATA') == [bytes([0, 0, 1, 1, 0]), bytes([66, 3, 1])]
assert values(0xC02, b'PRKC') == [b'\0']
assert values(0xC02, b'EPFD') == [struct.pack('<f', 1.25)]
assert values(0xC02, b'CTDA') == [struct.pack('<B3sfH2sIIIIi', 0x60, bytes(3), 1.0, 47, bytes(2), phial_master | 0x817, 0, 0, 0, -1)]
for n in [0xC03, 0xC05]:
    assert rec(n)[0][:4] == b'GLOB' and values(n, b'FLTV') == [struct.pack('<f', 0.0)]

class Reader:
    def __init__(self, b): self.b, self.p = b, 0
    def get(self, fmt):
        v = struct.unpack_from('<' + fmt, self.b, self.p)
        self.p += struct.calcsize('<' + fmt)
        return v[0] if len(v) == 1 else v
    def string(self):
        n = self.get('H'); v = self.b[self.p:self.p+n]; self.p += n
        return v.decode()
    def scripts(self):
        assert self.get('HH') == (5, 2)
        out = {}
        for _ in range(self.get('H')):
            name = self.string(); assert self.get('B') == 0
            props = {}
            for _ in range(self.get('H')):
                prop = self.string(); assert self.get('BB') == (1, 1)
                unused, alias, fid = self.get('HHI')
                assert unused == 0 and alias == 0xFFFF
                props[prop] = fid
            out[name] = props
        return out

reader = Reader(values(0xC01, b'VMAD')[0])
assert reader.scripts() == {'WPO_TraitEffect': {'Controller': own | 0xC04}}
assert reader.p == len(reader.b)
reader = Reader(values(0xC04, b'VMAD')[0])
assert reader.scripts() == {'WPO_Controller': {
    'TraitAbility': own | 0xC00, 'Chosen': own | 0xC03, 'GiftGranted': own | 0xC05,
    'ShopCell': 0x1678C, 'Phials': phial_master | 0x817, 'EmptyPhial': 0x2C25A,
    'PhialQuest': 0x1010AA, 'RefillHours': phial_master | 0x808}}
assert reader.get('BH') == (2, 0) and reader.string() == '' and reader.get('H') == 9
for alias in range(9):
    assert reader.get('HHI') == (0, alias, own | 0xC04)
    assert reader.scripts() == {('WPO_PlayerAlias' if alias == 0 else 'WPO_DoorAlias'): {}}
assert reader.p == len(reader.b)
assert values(0xC04, b'ALST') == [struct.pack('<I', i) for i in range(9)]
assert values(0xC04, b'ALFR') == [struct.pack('<I', 0x14)]
assert values(0xC04, b'FNAM') == [struct.pack('<I', 0x200)] + [struct.pack('<I', 0x202)] * 8
assert struct.unpack_from('<H', values(0xC04, b'DNAM')[0])[0] == 0x101
assert (STAGE / 'SEQ/Biggie Traits - Combined.seq').read_bytes() == struct.pack('<II', own | 0x923, own | 0xC04)

# The gift/list/cell bindings are verified against actual supplied plugin records.
source = parsed((ROOT.parent / 'white-phial-menu/original/The White Phial - Tweaks and Enhancements.esp').read_bytes())
assert source[0x2C25A][0][:4] == b'MISC'
phials = [struct.unpack('<I', v)[0] for t, v in source[0x05000817][1] if t == b'LNAM']
assert len(phials) == 10 and 0x2C25A in phials
assert set(phials) == {0x2C25A, *range(0x102019, 0x10201F), 0x05000807, 0x0500080A, 0x0500080C}
found_cell = False
for h, p in records((ROOT.parent / 'upload/03-Requiem.esp').read_bytes()):
    if struct.unpack_from('<I', h, 12)[0] == 0x1678C:
        assert h[:4] == b'CELL' and dict(fields(p))[b'EDID'] == b'WindhelmWhitePhial\0'
        found_cell = True
assert found_cell

lines = (STAGE / 'Biggie Traits - Combined_FLM.ini').read_text()
registration = re.findall(r'^FormList\s*=\s*(\w+)\|(\w+)', lines, re.M)
expected = {(kind, 'Traits_' + name + suffix) for kind, suffix in [('Traits_AbilityList', 'Ab'), ('Traits_EffectsList', '')]
            for name in ['GreybeardTrained', 'FullyDevoted', 'DevotedAlchemist', 'Skald', 'WhitePhialOwner']}
assert set(registration) == expected and len(registration) == 10
unchanged = []
for name, data in baseline.items():
    if name.startswith(('Scripts/', 'Source/Scripts/', 'Interface/', 'SKSE/')) or name.endswith('_KID.ini'):
        assert (STAGE / name).read_bytes() == data, name
        unchanged.append(name)
tex = (STAGE / 'Interface/TraitPics/Traits_WhitePhialOwnerAb.dds').read_bytes()
assert len(tex) == 721024 and tex[:4] == b'DDS ' and tex[84:88] == b'DXT5'
assert struct.unpack_from('<II', tex, 12) == (704, 1024)

scripts = {}
for stem in ['WPO_Controller', 'WPO_DoorAlias', 'WPO_PlayerAlias', 'WPO_TraitEffect']:
    assert (STAGE / 'Source/Scripts' / (stem + '.psc')).read_bytes() == (ROOT / 'papyrus' / (stem + '.psc')).read_bytes()
    pex = STAGE / 'Scripts' / (stem + '.pex')
    assert pex.read_bytes() == (ROOT / 'compiled' / pex.name).read_bytes()
    scripts[stem] = subprocess.check_output([str(ROOT.parent / 'fully-devoted/disassemble'), str(pex)], text=True)
c, d = scripts['WPO_Controller'], scripts['WPO_DoorAlias']
for text in ['callmethod RegisterForCrosshairRef', 'callstatic PO3_SKSEFunctions GetDoorDestination',
             'callmethod SetForRefill tracking', 'callmethod Start ::PhialQuest_var',
             'callmethod Stop acquisitionQuest', 'callmethod Stop repairQuest',
             'callstatic Quest GetQuest ::temp1 "MS12"', 'callstatic Quest GetQuest ::temp1 "MS12b"']:
    assert text in c, text
assert re.findall(r'callmethod SetValue ::Chosen_var .*', c) == ['callmethod SetValue ::Chosen_var ::NoneVar 1.0 ;1 variable args']
for text in ['callmethod BlockActivation doorRef ::NoneVar True', 'callmethod SetLockLevel doorRef ::NoneVar 255',
             '.function OnLockStateChanged', '.function OnActivate', 'akActionRef True ;2 variable args']:
    assert text in d, text
for text in ['callmethod SetStage ', 'callmethod SetCurrentStageID ', 'callmethod CompleteQuest ',
             'callmethod SetReward ', 'callmethod AddItem ']:
    assert all(text not in script for script in scripts.values()), text
assert 'callmethod Start ::Controller_var' in scripts['WPO_TraitEffect']

report = {'version': '2.1', 'single_esp': True, 'esl_flagged': True,
          'new_private_records': 6, 'previous_records_identical': 34, 'total_private_records': 40,
          'all_five_traits_registered': True, 'inventory_gated_alchemy_perk': 'validated',
          'empty_and_nine_filled_forms_in_original_list': 'validated',
          'quest_properties_and_nine_aliases': 'validated', 'compiled_papyrus_scripts': 4,
          'original_phial_scripts_replaced': False, 'existing_native_dll_changed': False,
          'unchanged_original_gameplay_files': unchanged, 'thumbnail': '1024x704 DXT5; visually checked',
          'new_powers_or_inventory_forms': 0, 'in_game_tested': False,
          'runtime_tests_needed': ['one-time gift and refill', 'crafting with/without empty/full phial',
              'shop at day/night; after waiting and save/load; with shop key', 'story quest lockout',
              'v2.0 save update; selecting the new trait starts its controller']}
(ROOT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
