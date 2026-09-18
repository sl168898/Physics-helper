"""Upgrade the delivered combined v1.5 without rebuilding unrelated traits."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = ROOT.parent
sys.path.insert(0, str(ROOT / 'tools'))
from esp import records, fields, field, record, group

BASE = ROOT / 'base/extracted'
STAGE = ROOT / 'staging'
PLUGIN = 'Biggie Traits - Devoted Alchemist.esp'
DESC = ('When you kill an enemy while your poison is active, <recover one dose> of '
        'the most recently applied poison still affecting them. Each enemy yields '
        'only one dose. Your applied poisons are <25% weaker>.')

shutil.copytree(BASE, STAGE, dirs_exist_ok=True)
source = list(records((BASE / PLUGIN).read_bytes()))
old = {int.from_bytes(h[12:16], 'little'): (h, p) for h, p in source}
assert set(old) == {0, *range(0x07000800, 0x07000805)}

def text(s): return s.encode('utf-8') + b'\0'
def replace_fields(payload, transform):
    return b''.join(field(t, transform(t, v)) for t, v in fields(payload))

groups = {}
for head, payload in source[1:]:
    form = int.from_bytes(head[12:16], 'little')
    parts = list(fields(payload))
    if form == 0x07000800:
        # Keep exactly the original identity and one main trait effect.
        assert parts[-4][0] == b'EFID' and parts[-2] == (b'EFID', struct.pack('<I', 0x07000803))
        parts = parts[:-2]
        parts = [(t, text('Venom Harvester') if t == b'FULL' else text(DESC) if t == b'DESC' else v)
                 for t, v in parts]
    elif form == 0x07000801:
        def main(t, v):
            if t == b'FULL': return text('Trait: Venom Harvester')
            if t == b'DNAM': return text('Recover one dose when you kill an enemy affected by your poison. Applied poisons are 25% weaker.')
            if t == b'DATA':
                data = bytearray(v)
                assert struct.unpack_from('<I', data, 136)[0] == 0x07000802
                struct.pack_into('<I', data, 136, 0)  # Retire crafting perk.
                return bytes(data)
            return v
        parts = [(t, main(t, v)) for t, v in parts]
    elif form == 0x07000802:
        # Neutralize a perk cached by an older save before migration runs.
        assert [v for t, v in parts if t == b'EPFD'] == [struct.pack('<f', 1.25)]
        parts = [(t, struct.pack('<f', 1.0) if t == b'EPFD' else v) for t, v in parts]
    payload = b''.join(field(t, v) for t, v in parts)
    groups.setdefault(head[:4], []).append(record(head[:4], form, payload, head=head))

glob = field(b'EDID', text('VH_MigrationVersion')) + field(b'FNAM', b'f') + field(b'FLTV', struct.pack('<f', 0))
groups[b'GLOB'].append(record(b'GLOB', 0x07000805, glob))
groups[b'FLST'] = [record(b'FLST', 0x07000806, field(b'EDID', text('VH_RetainedPoisons')))]
head, payload = source[0]
def header(t, v):
    if t == b'HEDR': return struct.pack('<fII', 1.7, 7 + len(groups), 0x807)
    if t == b'CNAM': return text('Venom Harvester addon')
    if t == b'SNAM': return text('Venom Harvester replacement; legacy trait identity retained for save upgrades.')
    return v
(STAGE / PLUGIN).write_bytes(record(b'TES4', 0, replace_fields(payload, header), head=head) +
                            b''.join(group(t, b''.join(rs)) for t, rs in groups.items()))

controller = STAGE / 'Source/Scripts/FD_PowerGrantAlias.psc'
script = controller.read_text()
before = '''    Spell alchemistTrait = Game.GetFormFromFile(0x800, "Biggie Traits - Devoted Alchemist.esp") as Spell
    If alchemistTrait && player.HasSpell(alchemistTrait)
        GrantAlchemyGiftOnce(player)
    EndIf
'''
assert script.count(before) == 1
script = script.replace(before, '''    MigrateVenomHarvester(player)
    VH_Native.Poll()
''')
start = script.index('Function GrantAlchemyGiftOnce(Actor player)')
assert script[start:].strip().endswith('EndFunction')
script = script[:start] + '''Function GrantAlchemyGiftOnce(Actor player)
    ; Retained as an inert legacy function for existing saves.
    Return
EndFunction

Function MigrateVenomHarvester(Actor player)
    GlobalVariable version = Game.GetFormFromFile(0x805, "Biggie Traits - Devoted Alchemist.esp") as GlobalVariable
    If !version || version.GetValue() >= 2.0
        Return
    EndIf
    Spell ability = Game.GetFormFromFile(0x800, "Biggie Traits - Devoted Alchemist.esp") as Spell
    Perk oldPerk = Game.GetFormFromFile(0x802, "Biggie Traits - Devoted Alchemist.esp") as Perk
    GlobalVariable gifts = Game.GetFormFromFile(0x804, "Biggie Traits - Devoted Alchemist.esp") as GlobalVariable
    If !ability || !oldPerk || !gifts
        Return
    EndIf
    gifts.SetValue(1.0)
    If version.GetValue() == 0.0
        If player.HasSpell(ability)
            ; Save selection before removing the cached old ability.
            version.SetValue(1.0)
        Else
            If player.HasPerk(oldPerk)
                player.RemovePerk(oldPerk)
            EndIf
            version.SetValue(2.0)
            Return
        EndIf
    EndIf
    If version.GetValue() == 1.0
        If player.HasSpell(ability)
            If !player.RemoveSpell(ability)
                Return
            EndIf
        EndIf
        ; Remove the old saved disease effect as well as the retired perk.
        player.DispelSpell(ability)
        If player.HasPerk(oldPerk)
            player.RemovePerk(oldPerk)
        EndIf
        player.AddSpell(ability, False)
        If player.HasSpell(ability)
            version.SetValue(2.0)
        EndIf
    EndIf
EndFunction
'''
controller.write_text(script)
native = 'Scriptname VH_Native Hidden\n\n; Implemented by VenomHarvester.dll; queues inventory work on the game thread.\nFunction Poll() Global Native\n'
(STAGE / 'Source/Scripts/VH_Native.psc').write_text(native)
compile_source = ROOT / 'papyrus'
compile_source.mkdir(exist_ok=True)
for name in ['FD_PowerGrantAlias.psc', 'VH_Native.psc']:
    shutil.copy2(STAGE / 'Source/Scripts' / name, compile_source / name)

new = {int.from_bytes(h[12:16], 'little'): (h, p) for h, p in records((STAGE / PLUGIN).read_bytes())}
assert set(new) == set(old) | {0x07000805, 0x07000806}
assert [v for t, v in fields(new[0][1]) if t == b'MAST'] == [v for t, v in fields(old[0][1]) if t == b'MAST']
assert [(t, v) for t, v in fields(new[0x07000800][1]) if t == b'EFID'] == [(b'EFID', struct.pack('<I', 0x07000801))]
assert struct.unpack_from('<I', dict(fields(new[0x07000801][1]))[b'DATA'], 136)[0] == 0
for id in [0x07000803, 0x07000804]: assert new[id] == old[id]
for id in range(0x07000800, 0x07000805):
    assert dict(fields(new[id][1]))[b'EDID'] == dict(fields(old[id][1]))[b'EDID']
print('PASS: 7 private records; old FormIDs/EditorIDs/masters retained; obsolete ability effect and perk detached.')
print('Prepared Papyrus migration and native interface.')
