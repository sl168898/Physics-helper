"""Change only the current Corpse Explosion's two descriptions to 50%."""
import hashlib
import struct
from esp import field, fields, records, record

BASELINE_SHA = '7397649668d13f924cbd2b462e3be21a602c24a8158d0518e215e5ca920bb2dd'
OWN = 0x21000000
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_SHA, 'Unexpected ESP baseline'
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    replacements = {}
    for fid, tag in ((OWN | 0xF80, b'DESC'), (OWN | 0xF81, b'DNAM')):
        original_fields = list(fields(before[fid][1]))
        description = dict(original_fields)[tag]
        assert description.count(b'25%') == 1 and b'25 feet (7.6 metres)' in description
        replacements[fid] = b''.join(field(t,
            v.replace(b'25%', b'50%') if t == tag else v)
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
    assert before.keys() == after.keys() and len(replacements) == 2
    for fid in before:
        if fid not in replacements:
            assert before[fid] == after[fid], hex(fid)
        else:
            assert after[fid][1] == replacements[fid] and before[fid] != after[fid]
            old_fields, new_fields = dict(fields(before[fid][1])), dict(fields(after[fid][1]))
            description_tag = b'DESC' if fid == OWN | 0xF80 else b'DNAM'
            assert new_fields[description_tag] == old_fields[description_tag].replace(b'25%', b'50%')
    return updated
