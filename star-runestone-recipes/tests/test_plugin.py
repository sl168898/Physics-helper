"""Independent checks of the actual binary records and reference structure."""
from pathlib import Path
import struct
import sys
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_plugin import make_plugin


def records(blob):
    offset = 0
    while offset < len(blob):
        tag, size, flags, form, _, version, _ = struct.unpack_from('<4sIIIIHH', blob, offset)
        if tag == b'GRUP':
            assert size >= 24 and offset + size <= len(blob)
            yield from records(blob[offset + 24:offset + size])
            offset += size
        else:
            assert offset + 24 + size <= len(blob)
            assert version == 44
            yield tag, flags, form, blob[offset + 24:offset + 24 + size]
            offset += 24 + size
    assert offset == len(blob)


def fields(blob):
    offset = 0
    while offset < len(blob):
        tag, size = struct.unpack_from('<4sH', blob, offset)
        assert offset + 6 + size <= len(blob)
        yield tag, blob[offset + 6:offset + 6 + size]
        offset += 6 + size
    assert offset == len(blob)


class PluginTests(unittest.TestCase):
    def test_records_and_masters(self):
        rs = list(records(make_plugin()))
        self.assertEqual(len(rs), 9)
        self.assertEqual([r[0] for r in rs], [b'TES4'] + [b'GLOB'] * 4 + [b'COBJ'] * 4)
        self.assertTrue(rs[0][1] & 0x200)
        fs = list(fields(rs[0][3]))
        self.assertEqual([v for k, v in fs if k == b'MAST'], [b'Skyrim.esm\0', b'RunemasterMagic.esl\0'])
        self.assertEqual(struct.unpack('<fII', dict(fs)[b'HEDR'])[1:], (10, 0x814))
        self.assertTrue(all((r[2] >> 24) == 2 and 0x800 <= (r[2] & 0xFFFFFF) < 0x1000 for r in rs[1:]))
        self.assertEqual(len({r[2] for r in rs}), 9)

    def test_exact_recipe_inputs_outputs_and_fail_closed_gates(self):
        rs = list(records(make_plugin()))
        gates = {form: (flags, dict(fields(data))) for tag, flags, form, data in rs if tag == b'GLOB'}
        for i, (tag, flags, form, data) in enumerate(r for r in rs if r[0] == b'COBJ'):
            fs = dict(fields(data))
            self.assertEqual(form, 0x02000800 + i)
            self.assertEqual(struct.unpack('<I', fs[b'COCT'])[0], 1)
            self.assertEqual(struct.unpack('<Ii', fs[b'CNTO']), (0x63B29 if i == 3 else 0x63B27, 1))
            self.assertEqual(struct.unpack('<I', fs[b'CNAM'])[0], 0x0100086F if i == 3 else 0x0100086E)
            self.assertEqual(struct.unpack('<H', fs[b'NAM1'])[0], [1, 2, 4, 4][i])
            self.assertEqual(struct.unpack('<I', fs[b'BNAM'])[0], 0x88105)
            self.assertEqual(len(fs[b'CTDA']), 32)
            self.assertEqual(fs[b'CTDA'][0], 0)  # exact equality
            self.assertEqual(struct.unpack_from('<f', fs[b'CTDA'], 4)[0], 1.)
            self.assertEqual(struct.unpack_from('<H', fs[b'CTDA'], 8)[0], 74)
            gate = struct.unpack_from('<I', fs[b'CTDA'], 12)[0]
            self.assertEqual(gate, 0x02000810 + i)
            self.assertTrue(gates[gate][0] & 0x40)
            self.assertEqual(gates[gate][1][b'FNAM'], b's')
            self.assertEqual(struct.unpack('<f', gates[gate][1][b'FLTV'])[0], 0.)


if __name__ == '__main__':
    unittest.main()
