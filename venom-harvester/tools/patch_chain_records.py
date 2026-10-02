"""Update only Corpse Explosion's two descriptions in the checked 2.13.7 ESP."""
import hashlib
import struct
from esp import field, fields, records, record

BASELINE_SHA = '2b0f566e77bdbafcb619be2b1a261f72799a83f0bc3396498a0e8410c9192dd1'
OWN = 0x21000000
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]
DESCRIPTION = (
    "Your poison or weapon oil's <killing blow> makes its corpse explode, dealing "
    "<50% of your damage to that enemy + 100> to enemies within <25 feet (7.6 metres)> "
    "in the killing coating's damage types. Enemies killed by a blast also explode "
    "with its lethal damage type. Each corpse explodes once. Your maximum Health "
    "is reduced by <50>."
)


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_SHA, 'Unexpected ESP baseline'
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    replacements = {}
    for fid, tag in ((OWN | 0xF80, b'DESC'), (OWN | 0xF81, b'DNAM')):
        old_fields = list(fields(before[fid][1]))
        old_text = dict(old_fields)[tag]
        assert old_text.count(b'50%') == 1 and b'No friendly fire or chain reactions.' in old_text
        text = DESCRIPTION if tag == b'DESC' else DESCRIPTION.replace('<', '').replace('>', '')
        replacements[fid] = b''.join(field(t, text.encode() + b'\0' if t == tag else v) for t, v in old_fields)

    def rewrite(blob):
        out = bytearray(); pos = 0
        while pos < len(blob):
            h = bytearray(blob[pos:pos + 24]); size = u32(h, 4)
            if h[:4] == b'GRUP':
                body = rewrite(blob[pos + 24:pos + size])
                struct.pack_into('<I', h, 4, len(body) + 24)
                out += h + body; pos += size
            else:
                fid = u32(h, 12)
                out += record(h[:4], fid, replacements[fid], head=h) if fid in replacements else blob[pos:pos + 24 + size]
                pos += 24 + size
        assert pos == len(blob)
        return bytes(out)

    updated = rewrite(original)
    after = {u32(h, 12): (h, b) for h, b in records(updated)}
    assert before.keys() == after.keys()
    assert {fid for fid in before if before[fid] != after[fid]} == set(replacements)
    for fid in replacements:
        tag = b'DESC' if fid == OWN | 0xF80 else b'DNAM'
        old_fields = list(fields(before[fid][1])); new_fields = list(fields(after[fid][1]))
        assert [(t, v) for t, v in old_fields if t != tag] == [(t, v) for t, v in new_fields if t != tag]
        text = dict(new_fields)[tag]
        assert b'50%' in text and b'+ 100' in text and b'also explode' in text
    return updated
