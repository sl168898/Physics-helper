"""Update the checked 2.13.3 ESP's descriptions and private blast visuals.

Magic Redone is already a master. Reuse its installed Poison Nova explosion
model without distributing third-party assets or changing any damage effects.
"""
import hashlib
import math
import struct
from esp import field, fields, records, record

BASELINE_SHA = '6cc6723234731d7679d9e182a8aea936b907193e6587cbf27d56271519695e2b'
OWN = 0x21000000
RADIUS_FEET = 25
RADIUS_UNITS = RADIUS_FEET * 128 / 6
POISON_MODEL = b'Noxcrab\\Restoration\\PoisonRuneExplosion.nif\0'
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_SHA, 'Unexpected ESP baseline'
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    masters = [v for t, v in fields(before[0][1]) if t == b'MAST']
    assert len(masters) == 33 and masters[24] == b'Requiem - Magic Redone.esp\0'
    replacements = {}
    for fid, tag in ((OWN | 0xF80, b'DESC'), (OWN | 0xF81, b'DNAM')):
        original_fields = list(fields(before[fid][1]))
        description = dict(original_fields)[tag]
        assert description.count(b'6 metres') == 1
        assert b'25%' in description and b'50' in description
        replacements[fid] = b''.join(field(t,
            v.replace(b'6 metres', b'25 feet (7.6 metres)') if t == tag else v)
            for t, v in original_fields)

    for i in range(4):
        fid = OWN | (0xF8A + i)
        assert before[fid][0][:4] == b'EXPL'
        original_fields = list(fields(before[fid][1]))
        ef = dict(original_fields)
        data = bytearray(ef[b'DATA'])
        assert len(data) == 52
        assert struct.unpack_from('<fff', data, 24) == (0, 0, 420)
        assert ef[b'MODL'] == b'Magic\\ShoutAreaExplosion01.nif\0'
        struct.pack_into('<f', data, 32, RADIUS_UNITS)
        replacements[fid] = b''.join(field(t,
            bytes(data) if t == b'DATA' else POISON_MODEL if i == 3 and t == b'MODL' else v)
            for t, v in original_fields)

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
    assert before.keys() == after.keys() and len(replacements) == 6
    for fid in before:
        if fid not in replacements:
            assert before[fid] == after[fid], hex(fid)
        else:
            assert after[fid][1] == replacements[fid] and before[fid] != after[fid]
    for i in range(4):
        ex = dict(fields(after[OWN | (0xF8A+i)][1]))
        assert struct.unpack_from('<ff', ex[b'DATA'], 24) == (0, 0)
        assert math.isclose(struct.unpack_from('<f', ex[b'DATA'], 32)[0], RADIUS_UNITS, abs_tol=0.0001)
        if i == 3:
            assert ex[b'MODL'] == POISON_MODEL
        effect = dict(fields(after[OWN | (0xF82+2*i)][1]))
        spell = dict(fields(after[OWN | (0xF83+2*i)][1]))
        assert u32(effect[b'DATA'], 76) == OWN | (0xF8A+i)
        assert u32(effect[b'DATA'], 84) == u32(spell[b'SPIT'], 20) == 0
        assert struct.unpack('<fII', spell[b'EFIT']) == (1.0, RADIUS_FEET, 0)
    assert [v for t, v in fields(after[0][1]) if t == b'MAST'] == masters
    return updated
