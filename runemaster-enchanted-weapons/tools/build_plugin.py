"""Build eight new hidden impact spells. No original records or assets copied.

Masters: Skyrim.esm (00), RunemasterMagic.esl (01); this light plugin is 02.
The DLL refreshes these effects from the winning rune enchantments at load time.
"""
import argparse
from pathlib import Path
import struct


def sub(tag, value):
    return struct.pack('<4sH', tag, len(value)) + value


def rec(tag, fid, data, flags=0):
    return struct.pack('<4sIIIIHH', tag, len(data), flags, fid, 0, 44, 0) + data


def make_plugin():
    # These are only references, not duplicated Runemaster scripts or artwork.
    impacts = [0x802, 0x809, 0x812, 0x817, 0x81D, 0x821, 0x825, 0x82A]
    names = ['StunningBlast', 'AlmightyBolt', 'Runestorm', 'SunderingInferno',
             'Havoc', 'SpectralSlash', 'UmbralVeil', 'BalefulGlow']
    header = sub(b'HEDR', struct.pack('<fII', 1.7, 9, 0x808))  # Eight records + one group.
    header += sub(b'CNAM', b'Physics-helper contributors\0')
    header += sub(b'SNAM', b'Additive rune impacts for RunemasterEnchantmentBridge; requires its DLL.\0')
    for master in ['Skyrim.esm', 'RunemasterMagic.esl']:
        header += sub(b'MAST', master.encode() + b'\0') + sub(b'DATA', bytes(8))
    result = rec(b'TES4', 0, header, 0x200)  # ESL flagged ESP; local forms 800-807.
    spells = b''
    for index, (name, impact) in enumerate(zip(names, impacts)):
        body = sub(b'EDID', ('REW_' + name + '_Impact').encode() + b'\0')
        body += sub(b'OBND', bytes(12))
        body += sub(b'FULL', ('Runemaster Bridge: ' + name).encode() + b'\0')
        # Zero-cost touch spell, instant native cast, no absorption of the carrier.
        # The original damage spells retain their own resistance/absorption rules.
        flags = 1 | (1 << 19) | (1 << 21) | (1 << 23)
        body += sub(b'SPIT', struct.pack('<IIIfIIffI', 0, flags, 0, 0., 1, 1, 0., 0., 0))
        body += sub(b'EFID', struct.pack('<I', 0x01000000 | impact))
        body += sub(b'EFIT', struct.pack('<fII', 10., 0, 1))
        spells += rec(b'SPEL', 0x02000800 + index, body)
    result += struct.pack('<4sI4sIHHHH', b'GRUP', 24 + len(spells), b'SPEL', 0, 0, 0, 0, 0) + spells
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    args.destination.parent.mkdir(parents=True, exist_ok=True)
    args.destination.write_bytes(make_plugin())
    print(f'Built {args.destination.name}: {len(make_plugin())} bytes, eight new spells, no overrides')
