"""Enable Skyrim's native area LOS check in the four private burst spells."""
import hashlib
import struct
from esp import field, fields, records, record

BASELINE_ESP_SHA = 'ec9b63375e79d24e93b5949e4279ddf5ae98d173dc517cfccf1a20998e1ba442'
SPELLS = tuple(0x21000F83 + 2*i for i in range(4))
IGNORE_LOS = 1 << 19
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_ESP_SHA
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    replacements = {}
    for fid in SPELLS:
        h, body = before[fid]
        assert h[:4] == b'SPEL'
        sf = dict(fields(body))
        spit = bytearray(sf[b'SPIT'])
        assert len(spit) == 36 and u32(spit, 4) == 0x3C0001
        assert u32(spit, 20) == 0 # Self
        assert struct.unpack('<fII', sf[b'EFIT']) == (1.0, 25, 0)
        struct.pack_into('<I', spit, 4, u32(spit, 4) & ~IGNORE_LOS)
        replacements[fid] = b''.join(field(t, bytes(spit) if t == b'SPIT' else v) for t, v in fields(body))

    def rewrite(blob):
        out = bytearray(); pos = 0
        while pos < len(blob):
            h = bytearray(blob[pos:pos+24]); size = u32(h, 4)
            if h[:4] == b'GRUP':
                body = rewrite(blob[pos+24:pos+size])
                struct.pack_into('<I', h, 4, len(body)+24)
                out += h + body; pos += size
            else:
                fid = u32(h, 12)
                out += record(h[:4], fid, replacements[fid], head=h) if fid in replacements else blob[pos:pos+24+size]
                pos += 24+size
        assert pos == len(blob)
        return bytes(out)

    updated = rewrite(original)
    after = {u32(h, 12): (h, b) for h, b in records(updated)}
    assert before.keys() == after.keys()
    assert {fid for fid in before if before[fid] != after[fid]} == set(SPELLS)
    for fid in SPELLS:
        oldfields, newfields = list(fields(before[fid][1])), list(fields(after[fid][1]))
        assert len(oldfields) == len(newfields)
        for (ot, ov), (nt, nv) in zip(oldfields, newfields):
            assert ot == nt
            if ot == b'SPIT':
                expected = bytearray(ov)
                struct.pack_into('<I', expected, 4, 0x340001)
                assert nv == expected
                assert not u32(nv, 4) & IGNORE_LOS
                assert u32(nv, 4) & (1 << 20) and u32(nv, 4) & (1 << 21) # Once-only resistance; no absorption.
            else: assert ov == nv
    return updated
