"""Generate four new forge recipes and four fail-closed condition globals.

Masters: Skyrim.esm (00), RunemasterMagic.esl (01); this light plugin is 02.
No original records, artwork, scripts, spells, or quests are copied or edited.
"""
import argparse
from pathlib import Path
import struct


def sub(tag, value):
    return struct.pack('<4sH', tag, len(value)) + value


def record(tag, fid, data, flags=0):
    return struct.pack('<4sIIIIHH', tag, len(data), flags, fid, 0, 44, 0) + data


def group(tag, data):
    return struct.pack('<4sI4sIHHHH', b'GRUP', 24 + len(data), tag, 0, 0, 0, 0, 0) + data


def make_plugin():
    header = sub(b'HEDR', struct.pack('<fII', 1.7, 10, 0x814))  # 8 records + 2 groups.
    header += sub(b'CNAM', b'Physics-helper contributors\0')
    header += sub(b'SNAM', b'Craft Runemaster runestones using a Star soul; retain the empty Star. Requires StarRunestoneRecipes.dll.\0')
    for master in ['Skyrim.esm', 'RunemasterMagic.esl']:
        header += sub(b'MAST', master.encode() + b'\0') + sub(b'DATA', bytes(8))
    plugin = record(b'TES4', 0, header, 0x200)
    names = ['AzuraCommon', 'AzuraGreater', 'AzuraGrand', 'BlackGrand']
    stars = [0x63B27, 0x63B27, 0x63B27, 0x63B29]
    outputs = [0x0100086E, 0x0100086E, 0x0100086E, 0x0100086F]
    amounts = [1, 2, 4, 4]
    globals_data = b''
    recipes_data = b''
    for i, name in enumerate(names):
        gate = 0x02000810 + i
        data = sub(b'EDID', ('SRR_Available' + name).encode() + b'\0')
        data += sub(b'FNAM', b's') + sub(b'FLTV', struct.pack('<f', 0.))
        globals_data += record(b'GLOB', gate, data, 0x40)  # constant zero fallback
        data = sub(b'EDID', ('SRR_Recipe' + name).encode() + b'\0')
        data += sub(b'COCT', struct.pack('<I', 1))
        data += sub(b'CNTO', struct.pack('<Ii', stars[i], 1))
        # GetGlobalValue(private marker) == 1. The native condition adapter
        # returns current soul eligibility. Without the DLL this is false.
        condition = struct.pack('<B3xfH2xIIIIi', 0, 1., 74, gate, 0, 0, 0, -1)
        assert len(condition) == 32
        data += sub(b'CTDA', condition)
        data += sub(b'CNAM', struct.pack('<I', outputs[i]))
        data += sub(b'BNAM', struct.pack('<I', 0x88105))  # CraftingSmithingForge
        data += sub(b'NAM1', struct.pack('<H', amounts[i]))
        recipes_data += record(b'COBJ', 0x02000800 + i, data)
    return plugin + group(b'GLOB', globals_data) + group(b'COBJ', recipes_data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    result = make_plugin()
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(result)
    print(f'Built {args.destination.name}: {len(result)} bytes; 4 recipes; 4 condition markers; no overrides')
