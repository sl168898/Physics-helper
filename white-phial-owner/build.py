"""Append White Phial Owner to v2.0; preserve every existing gameplay record."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'venom-harvester/tools'))
from esp import records, fields, field, record, group
BASE = ROOT / 'input/Biggie_Traits_Combined_Single_ESP_v2_0.zip'
STAGE = ROOT / 'staging'
NAME = 'Biggie Traits - Combined.esp'
sha = lambda b: hashlib.sha256(b).hexdigest()
assert sha(BASE.read_bytes()) == 'e9d0e24f8a0bf875a3eda10b15a9df94d30908b729fdaf269d2ff5ed4ec562d6'
with zipfile.ZipFile(BASE) as z:
    assert z.testzip() is None
    baseline = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    for name, data in baseline.items():
        target = STAGE / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
original = list(records(baseline[NAME]))
masters = [v.rstrip(b'\0').decode() for t, v in fields(original[0][1]) if t == b'MAST']
own = len(masters) << 24
phial_master = masters.index('The White Phial - Tweaks and Enhancements.esp') << 24
ids = {name: own | (0xC00 + i) for i, name in enumerate(['Ability', 'Effect', 'Perk', 'Chosen', 'Controller', 'GiftGranted'])}
def s(t, value): return field(t.encode(), value.encode() + b'\0')
def u(t, value): return field(t.encode(), struct.pack('<I', value))
def ws(value):
    value = value.encode()
    return struct.pack('<H', len(value)) + value
def vm(name, props):
    return (struct.pack('<HHH', 5, 2, 1) + ws(name) + b'\0' + struct.pack('<H', len(props)) +
            b''.join(ws(key) + b'\x01\x01' + struct.pack('<HHI', 0, 0xFFFF, value) for key, value in props))
def cond(fn, param, value=1.0, flags=0):
    return field(b'CTDA', struct.pack('<B3sfH2sIIIIi', flags, bytes(3), value, fn, bytes(2), param, 0, 0, 0, -1))

old_by_id = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in original}
voice = dict(fields(old_by_id[own | 0x800][1]))
data = bytearray(dict(fields(old_by_id[own | 0x801][1]))[b'DATA'])
assert len(data) == 152
struct.pack_into('<I', data, 136, ids['Perk'])
description = ('Receive <one empty White Phial>. While carrying the White Phial, filled or empty, '
               'potions and poisons you make are <25% stronger>. The White Phial shop is '
               '<permanently closed to you>; its entrance cannot be lockpicked or opened with a key. '
               'You forfeit its acquisition and repair quests and their rewards.')
effect = (s('EDID', 'Traits_WhitePhialOwner') + field(b'VMAD', vm('WPO_TraitEffect', [('Controller', ids['Controller'])])) +
          s('FULL', 'Trait: The White Phial Owner') + u('MDOB', 0x435A5) + field(b'DATA', bytes(data)) +
          field(b'SNDD', b'') + s('DNAM', 'Potions and poisons you make are 25% stronger while carrying the White Phial. Its shop is permanently closed to you.'))
ability = (s('EDID', 'Traits_WhitePhialOwnerAb') + field(b'OBND', bytes(12)) + s('FULL', 'The White Phial Owner') +
           u('ETYP', 0x13F44) + s('DESC', description) + field(b'SPIT', voice[b'SPIT']) +
           u('EFID', ids['Effect']) + field(b'EFIT', bytes(12)))
# Entry point 66, multiply effectiveness; no potion/poison filter. Evaluate
# GetItemCount against the original mod's phial list at crafting time.
perk = (s('EDID', 'WPO_AlchemyPerk') + s('DESC', '') + field(b'DATA', bytes([0, 0, 1, 1, 0])) +
        field(b'PRKE', bytes([2, 0, 0])) + field(b'DATA', bytes([66, 3, 1])) + field(b'PRKC', b'\0') +
        cond(47, phial_master | 0x817, 1.0, 0x60) + field(b'EPFT', b'\1') +
        field(b'EPFD', struct.pack('<f', 1.25)) + field(b'PRKF', b''))

props = [('TraitAbility', ids['Ability']), ('Chosen', ids['Chosen']), ('GiftGranted', ids['GiftGranted']),
         ('ShopCell', 0x1678C), ('Phials', phial_master | 0x817), ('EmptyPhial', 0x2C25A),
         ('PhialQuest', 0x1010AA), ('RefillHours', phial_master | 0x808)]
qvm = vm('WPO_Controller', props) + struct.pack('<BH', 2, 0) + ws('') + struct.pack('<H', 9)
for alias_id in range(9):
    qvm += struct.pack('<HHI', 0, alias_id, ids['Controller'])
    qvm += vm('WPO_PlayerAlias' if alias_id == 0 else 'WPO_DoorAlias', [])
quest = s('EDID', 'WPO_ControllerQuest') + field(b'VMAD', qvm)
quest += field(b'DNAM', struct.pack('<HBB4sI', 0x101, 5, 0, bytes(4), 0)) + field(b'NEXT', b'') + u('ANAM', 9)
for alias_id in range(9):
    quest += u('ALST', alias_id) + s('ALID', 'Player' if alias_id == 0 else f'ShopEntrance{alias_id}')
    quest += u('FNAM', 0x200 if alias_id == 0 else 0x202)
    if alias_id == 0: quest += u('ALFR', 0x14)
    quest += field(b'ALED', b'')

new = [(b'SPEL', 'Ability', ability), (b'MGEF', 'Effect', effect), (b'PERK', 'Perk', perk),
       (b'QUST', 'Controller', quest)]
for label in ['Chosen', 'GiftGranted']:
    new.append((b'GLOB', label, s('EDID', 'WPO_' + label) + field(b'FNAM', b's') + field(b'FLTV', struct.pack('<f', 0.0))))
out = {}
for h, b in original[1:]: out.setdefault(h[:4], []).append(record(h[:4], 0, b, head=h))
for tag, key, b in new: out.setdefault(tag, []).append(record(tag, ids[key], b))
header = b''
for tag, value in fields(original[0][1]):
    if tag == b'HEDR': value = struct.pack('<fII', 1.7, len(original) - 1 + len(new), 0xC06)
    if tag == b'CNAM': value = b'Biggie Traits Combined\0'
    if tag == b'SNAM': value = b'Five custom traits, one ESL-flagged ESP. Adds The White Phial Owner.\0'
    header += field(tag, value)
merged = record(b'TES4', 0, header, flags=0x200) + b''.join(group(tag, b''.join(rs)) for tag, rs in out.items())
(STAGE / NAME).write_bytes(merged)
(STAGE / 'Biggie Traits - Combined_FLM.ini').write_bytes(baseline['Biggie Traits - Combined_FLM.ini'] +
    b'FormList = Traits_AbilityList|Traits_WhitePhialOwnerAb\nFormList = Traits_EffectsList|Traits_WhitePhialOwner\n')
seq = 'SEQ/Biggie Traits - Combined.seq'
(STAGE / seq).write_bytes(baseline[seq] + struct.pack('<I', ids['Controller']))
for src in (ROOT / 'papyrus').glob('*.psc'): shutil.copyfile(src, STAGE / 'Source/Scripts' / src.name)
report = {'version': '2.1', 'trait': 'The White Phial Owner', 'plugin': NAME,
          'new_records': {k: f'{v:08X}' for k, v in ids.items()}, 'new_record_count': len(new),
          'total_private_records': 40, 'masters_unchanged': masters, 'existing_records_unchanged': 34,
          'alchemy_multiplier': 1.25, 'crafting_condition': 'GetItemCount(original phial FormList) >= 1',
          'filled_phials_and_empty_supported': True, 'gift': 'Original empty phial, once per save; no duplicate if already owned',
          'shop_cell_verified_from': 'Uploaded Requiem.esp: WindhelmWhitePhial [CELL:0001678C]',
          'door_resolution': 'powerofthree GetDoorDestination; match destination parent cell',
          'door_lock_level': 255, 'activation_block': 'player only; NPC default processing allowed',
          'permanent_after_selection': True, 'quests_stopped_if_running_and_incomplete': ['MS12', 'MS12b'],
          'refill_service_kept_running': 'MS12PostQuest', 'quest_rewards_granted': False,
          'fully_enchanted_setting_changed': False, 'no_new_inventory_items_or_powers': True,
          'in_game_tested': False}
(ROOT / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
