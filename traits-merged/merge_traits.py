"""Merge the exact four v1.6 trait ESPs using typed FormID fields, never byte search."""
from pathlib import Path
import collections
import hashlib
import json
import re
import shutil
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parent
sys.path.insert(0, str(WORKSPACE / 'venom-harvester/tools'))
from esp import records, fields, field, record, group

SOURCE_ZIP = WORKSPACE / 'outputs/Biggie_Traits_Combined_v1_6.zip'
STAGE = ROOT / 'staging'
NAME = 'Biggie Traits - Combined.esp'
SOURCES = {
    'Biggie Traits - Greybeard Trained.esp': 0x800,
    'Biggie Traits - Fully Devoted.esp': 0x900,
    'Biggie Traits - Devoted Alchemist.esp': 0xA00,
    'Biggie Traits - Skald.esp': 0xB00,
}
assert hashlib.sha256(SOURCE_ZIP.read_bytes()).hexdigest() == '1740adef87fa3e4a28fbdc2806f0ef0408f6d08e148b3f08b5c5289d060dcf57'
with zipfile.ZipFile(SOURCE_ZIP) as z:
    assert z.testzip() is None
    original = {n: z.read(n) for n in z.namelist()}

parsed = {name: list(records(original[name])) for name in SOURCES}
masters = {name: [v.rstrip(b'\0').decode() for t, v in fields(rs[0][1]) if t == b'MAST']
           for name, rs in parsed.items()}
merged_masters = list(masters['Biggie Traits - Fully Devoted.esp'])
for names in masters.values():
    for name in names:
        if name not in merged_masters and name not in SOURCES: merged_masters.append(name)
assert not set(SOURCES) & set(merged_masters)
own = len(merged_masters) << 24

mapping = {}
edids = set()
for name, rs in parsed.items():
    for head, payload in rs[1:]:
        old = struct.unpack_from('<I', head, 12)[0]
        assert old >> 24 == len(masters[name]), 'Unexpected override: manual conflict resolution needed'
        local = (old & 0xFFFFFF) - 0x800 + SOURCES[name]
        assert 0x800 <= local <= 0xFFF
        mapping[name, old] = own | local
        edid = dict(fields(payload))[b'EDID']
        assert edid not in edids, 'Ambiguous EditorID'
        edids.add(edid)
assert len(set(mapping.values())) == len(mapping) == 34

references = []
def remap(source, old, path):
    if old == 0: return 0
    index, local = old >> 24, old & 0xFFFFFF
    if index == len(masters[source]):
        assert (source, old) in mapping, (source, path, hex(old), 'missing private form')
        new = mapping[source, old]
        owner = NAME
    else:
        assert index < len(masters[source]), (source, path, hex(old), 'invalid master index')
        owner = masters[source][index]
        new = (merged_masters.index(owner) << 24) | local
    references.append({'source': source, 'field': path, 'old': f'{old:08X}', 'new': f'{new:08X}', 'owner': owner})
    return new

def offset_refs(source, value, offsets, path):
    result = bytearray(value)
    for offset in offsets:
        assert offset + 4 <= len(result)
        old = struct.unpack_from('<I', result, offset)[0]
        struct.pack_into('<I', result, offset, remap(source, old, f'{path}+{offset}'))
    return bytes(result)

class VMAD:
    def __init__(self, source, value, kind):
        self.source, self.data, self.pos, self.kind = source, bytearray(value), 0, kind
    def get(self, fmt):
        size = struct.calcsize('<' + fmt)
        assert self.pos + size <= len(self.data)
        value = struct.unpack_from('<' + fmt, self.data, self.pos)
        self.pos += size
        return value[0] if len(value) == 1 else value
    def string(self):
        length = self.get('H')
        assert self.pos + length <= len(self.data)
        out = bytes(self.data[self.pos:self.pos + length]).decode()
        self.pos += length
        return out
    def object(self, fmt, path):
        assert fmt in (1, 2)
        offset = self.pos + (4 if fmt == 2 else 0)
        old = struct.unpack_from('<I', self.data, offset)[0]
        struct.pack_into('<I', self.data, offset, remap(self.source, old, path))
        self.pos += 8
    def value(self, kind, fmt, path):
        if kind == 1: self.object(fmt, path)
        elif kind == 2: self.string()
        elif kind in (3, 4): self.get('I')
        elif kind == 5: self.get('B')
        elif 11 <= kind <= 15:
            for i in range(self.get('I')): self.value(kind - 10, fmt, f'{path}[{i}]')
        else: raise AssertionError(('unsupported VMAD property', kind))
    def scripts(self, count, fmt):
        for _ in range(count):
            name = self.string(); self.get('B')
            for _ in range(self.get('H')):
                prop = self.string(); kind, flags = self.get('BB')
                self.value(kind, fmt, f'VMAD/{name}/{prop}')
    def run(self):
        version, fmt, count = self.get('HHH')
        assert version == 5 and fmt == 2
        self.scripts(count, fmt)
        if self.kind == b'QUST':
            assert self.get('B') == 2
            fragments = self.get('H'); self.string()
            assert fragments == 0, 'Unexpected quest fragments need explicit handling'
            aliases = self.get('H')
            for i in range(aliases):
                self.object(fmt, f'VMAD/Alias[{i}]/Quest')
                aversion, afmt, count = self.get('HHH')
                assert aversion == 5 and afmt == 2
                self.scripts(count, afmt)
        assert self.pos == len(self.data), (self.kind, self.pos, len(self.data))
        return bytes(self.data)

NO_REFERENCES = {
    b'KYWD': {b'EDID', b'CNAM'},
    b'GLOB': {b'EDID', b'FNAM', b'FLTV'},
    b'MGEF': {b'EDID', b'FULL', b'DNAM'},
    b'SPEL': {b'EDID', b'OBND', b'FULL', b'DESC', b'EFIT'},
    b'PERK': {b'EDID', b'DESC', b'DATA', b'PRKE', b'PRKC', b'EPFT', b'EPFD', b'PRKF'},
    b'QUST': {b'EDID', b'DNAM', b'NEXT', b'ANAM', b'ALST', b'ALID', b'FNAM', b'ALED'},
    b'MESG': {b'EDID', b'DESC', b'FULL', b'DNAM', b'ITXT'},
    b'FLST': {b'EDID'},
}

def map_condition(source, value, path):
    assert len(value) == 32
    flags = value[0]
    fn = struct.unpack_from('<H', value, 8)[0]
    # Current records use these ten functions. Actor-value enums, spell types,
    # floats and booleans MUST NOT be mistaken for FormIDs.
    numeric = {46, 108, 277, 353, 432, 500, 673, 719}
    first_form = {693, 697}  # EPMagic_SpellHasKeyword; IsAttackType (AACT).
    assert fn in numeric | first_form, ('unknown condition signature', fn)
    assert struct.unpack_from('<I', value, 16)[0] == 0
    offsets = [24]  # Explicit run-on reference; zero in the supplied records.
    if fn in first_form: offsets.append(12)
    if flags & 4: offsets.append(4)  # Comparison global.
    return offset_refs(source, value, offsets, f'{path}/fn{fn}')

def map_payload(source, kind, data):
    out = []
    perk_entry = None
    for tag, value in fields(data):
        path = f'{kind.decode()}/{tag.decode()}'
        if tag == b'VMAD': value = VMAD(source, value, kind).run()
        elif tag == b'CTDA': value = map_condition(source, value, path)
        elif tag in {b'EFID', b'ETYP', b'MDOB', b'ALFR', b'LNAM', b'INAM'}:
            assert len(value) == 4
            value = offset_refs(source, value, [0], path)
        elif kind == b'MGEF' and tag == b'DATA':
            assert len(value) == 152
            value = offset_refs(source, value,
                [8, 24, 32, 36, 72, 76, 92, 96, 100, 108, 116, 120, 124, 128, 132, 136], path)
        elif kind == b'MGEF' and tag == b'SNDD':
            assert len(value) % 8 == 0
            value = offset_refs(source, value, range(4, len(value), 8), path)
        elif kind == b'SPEL' and tag == b'SPIT':
            assert len(value) == 36
            value = offset_refs(source, value, [32], path)
        else:
            assert tag in NO_REFERENCES[kind], ('unhandled field', kind, tag)
            if kind == b'PERK':
                if tag == b'PRKE':
                    assert len(value) == 3 and value[0] == 2  # Entry point, not quest/ability.
                    perk_entry = 2
                elif tag == b'DATA':
                    if perk_entry is None: assert len(value) == 5
                    else: assert len(value) == 3 and value[1] == 3  # Multiply value.
                elif tag == b'EPFT': assert value == b'\x01'  # One float, no spell/form.
                elif tag == b'EPFD': assert len(value) == 4
        out.append(field(tag, value))
    return b''.join(out)

output = collections.defaultdict(list)
source_report = []
for name, rs in parsed.items():
    for head, data in rs[1:]:
        old = struct.unpack_from('<I', head, 12)[0]
        new = mapping[name, old]
        mapped = map_payload(name, head[:4], data)
        new_head = bytearray(head)
        struct.pack_into('<I', new_head, 12, new)
        output[head[:4]].append(record(head[:4], new, mapped, head=new_head))
        source_report.append({'source': name, 'old': f'{old:08X}', 'new': f'{new:08X}',
                              'local': f'{new & 0xFFFFFF:06X}', 'type': head[:4].decode(),
                              'editor_id': dict(fields(data))[b'EDID'].rstrip(b'\0').decode()})

header = field(b'HEDR', struct.pack('<fII', 1.7, len(mapping) + len(output), 0xB06))
header += field(b'CNAM', b'Biggie Traits Combined\0')
header += field(b'SNAM', b'Single-plugin edition: Voice of Authority, Fully Devoted, Venom Harvester and Skald.\0')
for master in merged_masters:
    header += field(b'MAST', master.encode() + b'\0') + field(b'DATA', bytes(8))
merged = record(b'TES4', 0, header, flags=0x200)
for kind in [b'KYWD', b'GLOB', b'MGEF', b'SPEL', b'PERK', b'QUST', b'MESG', b'FLST']:
    merged += group(kind, b''.join(output[kind]))

STAGE.mkdir(parents=True, exist_ok=True)
# Only the new ESP and merged distribution files will be enabled. Keep artwork,
# original effect scripts, source and license credits; native DLL is installed
# separately only after its new Windows build is verified.
for name, data in original.items():
    if name.endswith(('.esp', '.ini', '.seq', '.dll')) or name in {'README.txt', 'Validation.json'}: continue
    if name.startswith('Documentation/'): continue
    dest = STAGE / name; dest.parent.mkdir(parents=True, exist_ok=True); dest.write_bytes(data)
(STAGE / NAME).write_bytes(merged)
formlists = ''.join(original[n].decode().rstrip() + '\n' for n in original if n.endswith('_FLM.ini'))
(STAGE / 'Biggie Traits - Combined_FLM.ini').write_text(formlists)
(STAGE / 'Biggie Traits - Combined_KID.ini').write_bytes(original['Biggie Traits - Greybeard Trained_KID.ini'])
(STAGE / 'SEQ').mkdir(exist_ok=True)
quest = mapping['Biggie Traits - Fully Devoted.esp', 0x20000823]
(STAGE / 'SEQ/Biggie Traits - Combined.seq').write_bytes(struct.pack('<I', quest))

script_references = []
for file in (STAGE / 'Source/Scripts').glob('*.psc'):
    before = file.read_text()
    def change(match):
        local = int(match.group(1), 16); source = match.group(2)
        if source not in SOURCES: return match.group(0)
        old = (len(masters[source]) << 24) | (local & 0xFFFFFF)
        target = mapping[source, old] & 0xFFFFFF
        script_references.append({'script': file.name, 'source': source, 'old_local': f'{local:X}', 'new_local': f'{target:X}'})
        return f'Game.GetFormFromFile(0x{target:X}, "{NAME}")'
    after = re.sub(r'Game\.GetFormFromFile\(0x([0-9a-fA-F]+), "([^"]+)"\)', change, before)
    if before != after:
        file.write_text(after)
        papyrus = ROOT / 'papyrus'; papyrus.mkdir(exist_ok=True)
        (papyrus / file.name).write_text(after)
    for source in SOURCES: assert source not in after, (file.name, 'stale plugin name')

docs = STAGE / 'Documentation'; docs.mkdir(exist_ok=True)
(docs / 'FormID-Map.json').write_text(json.dumps({'plugin': NAME, 'masters': merged_masters,
    'records': source_report, 'references': references, 'script_references': script_references}, indent=2) + '\n')
(ROOT / 'merge-report.json').write_text(json.dumps({'plugin': NAME, 'records': len(mapping),
    'esl_flagged': True, 'new_record_range': '0x800-0xB05', 'references_checked': len(references),
    'script_lookups_remapped': len(script_references), 'old_plugins_are_masters': False,
    'requires_fresh_save': True, 'in_game_tested': False}, indent=2) + '\n')

# Verify both ownership and all numeric/text payload bytes independently by
# reversing the known reference transformations in each individual subrecord.
merged_records = {int.from_bytes(h[12:16], 'little'): (h, p) for h, p in records(merged)}
assert set(merged_records) == {0, *mapping.values()}
for row in source_report:
    source = row['source']; old_id = int(row['old'], 16); new_id = int(row['new'], 16)
    old_h, old_p = next((h, p) for h, p in parsed[source] if int.from_bytes(h[12:16], 'little') == old_id)
    new_h, new_p = merged_records[new_id]
    assert new_h[:12] == old_h[:12] and new_h[16:] == old_h[16:]
    assert map_payload(source, old_h[:4], old_p) == new_p
    for (old_tag, old_v), (new_tag, new_v) in zip(fields(old_p), fields(new_p)):
        assert old_tag == new_tag and len(old_v) == len(new_v)
        if old_tag in NO_REFERENCES[old_h[:4]]: assert old_v == new_v
assert len(script_references) == 7
print(f'PASS: {len(mapping)} records merged; 1 ESL-flagged ESP; 7 script lookups remapped; original gameplay values preserved.')
