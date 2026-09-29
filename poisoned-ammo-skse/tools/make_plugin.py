"""Build and independently validate the static form pool. Python 3, no packages.

Record layouts: TES5Edit/Core/wbDefinitionsTES5.pas (AMMO, ALCH, GLOB, TES4).
Only our own records are created. No Skyrim records or assets are redistributed.
"""
from pathlib import Path
import argparse
import struct

CAPACITY = 512
OWN = 0x01000000  # One master, Skyrim.esm. The loader assigns the light index.

def sub(kind, data):
    return kind.encode('ascii') + struct.pack('<H', len(data)) + data

def text(kind, value):
    return sub(kind, value.encode('utf-8') + b'\0')

def record(kind, ident, body, flags=0):
    return struct.pack('<4sIIIIHH', kind.encode('ascii'), len(body), flags, ident, 0, 44, 0) + body

def group(kind, records):
    body = b''.join(records)
    return struct.pack('<4sI4sIHHI', b'GRUP', len(body) + 24, kind.encode('ascii'), 0, 0, 0, 0) + body

def build():
    ammo, poisons = [], []
    for i in range(CAPACITY):
        ammo.append(record('AMMO', OWN + 0x800 + i,
            text('EDID', f'PAN_Ammo_{i:03d}') + sub('OBND', bytes(12)) +
            text('FULL', 'Unassigned poisoned ammunition') +
            sub('DATA', struct.pack('<IIfIf', 0, 6, 0.0, 0, 0.0))))
        poisons.append(record('ALCH', OWN + 0xA00 + i,
            text('EDID', f'PAN_Poison_{i:03d}') + sub('OBND', bytes(12)) +
            text('FULL', 'Poisoned ammunition effect proxy') + sub('DATA', struct.pack('<f', 0.0)) +
            sub('ENIT', struct.pack('<iIIfI', 0, 1 << 17, 0, 0.0, 0))))
    glob = record('GLOB', OWN + 0xC00, text('EDID', 'PAN_SaveFingerprint') +
        sub('FNAM', b'f') + sub('FLTV', struct.pack('<f', 0.0)))
    header = record('TES4', 0,
        sub('HEDR', struct.pack('<fII', 1.7, CAPACITY * 2 + 1, 0xC01)) +
        text('CNAM', 'Physics-helper contributors') +
        text('SNAM', 'Poisoned Ammo Native 0.1.0. Static ESL form pool; requires its SKSE DLL.') +
        text('MAST', 'Skyrim.esm') + sub('DATA', bytes(8)), 0x200)
    return header + group('GLOB', [glob]) + group('ALCH', poisons) + group('AMMO', ammo)

def validate(blob):
    """Walk all records and subrecords, checking sizes, IDs, flags and group tags."""
    records = []
    def walk(start, end, expected=None):
        pos = start
        while pos < end:
            assert pos + 24 <= end
            sig, size = struct.unpack_from('<4sI', blob, pos)
            if sig == b'GRUP':
                assert size >= 24 and pos + size <= end
                label, kind = struct.unpack_from('<4sI', blob, pos + 8)
                assert kind == 0
                walk(pos + 24, pos + size, label)
                pos += size
                continue
            assert expected is None or sig == expected
            assert pos + 24 + size <= end
            flags, fid = struct.unpack_from('<II', blob, pos + 8)
            data = {}
            subpos, stop = pos + 24, pos + 24 + size
            while subpos < stop:
                kind, length = struct.unpack_from('<4sH', blob, subpos)
                subpos += 6
                assert subpos + length <= stop
                assert kind not in data
                data[kind] = blob[subpos:subpos + length]
                subpos += length
            assert subpos == stop
            records.append((sig, flags, fid, data))
            pos = stop
        assert pos == end
    walk(0, len(blob))
    assert len(records) == CAPACITY * 2 + 2
    header = records[0]
    assert header[:3] == (b'TES4', 0x200, 0)
    version, count, next_id = struct.unpack('<fII', header[3][b'HEDR'])
    assert 1.69 < version < 1.71 and count == len(records) - 1 and next_id == 0xC01
    assert header[3][b'MAST'] == b'Skyrim.esm\0'
    ids = [r[2] for r in records[1:]]
    assert len(set(ids)) == len(ids) and all(OWN + 0x800 <= fid <= OWN + 0xC00 for fid in ids)
    for sig, flags, fid, data in records[1:]:
        assert flags == 0
        if sig == b'AMMO':
            assert 0x800 <= fid - OWN < 0xA00
            assert len(data[b'OBND']) == 12 and len(data[b'DATA']) == 20
            assert struct.unpack('<IIfIf', data[b'DATA']) == (0, 6, 0.0, 0, 0.0)
        elif sig == b'ALCH':
            assert 0xA00 <= fid - OWN < 0xC00
            assert len(data[b'ENIT']) == 20
            assert struct.unpack('<iIIfI', data[b'ENIT'])[1] == 1 << 17
        else:
            assert sig == b'GLOB' and fid == OWN + 0xC00
            assert data[b'FNAM'] == b'f' and struct.unpack('<f', data[b'FLTV'])[0] == 0.0
    return len(records) - 1

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    content = build()
    count = validate(content)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(content)
    assert validate(args.output.read_bytes()) == count
    print(f'PASS: {count} unique ESL records; {len(content)} bytes; output={args.output}')
