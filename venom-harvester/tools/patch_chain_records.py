"""Update descriptions and add 16 budget-coded private spells to the checked ESP."""
import hashlib
import struct
from esp import field, fields, records, record

BASELINE_SHA = '2b0f566e77bdbafcb619be2b1a261f72799a83f0bc3396498a0e8410c9192dd1'
OWN = 0x21000000
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]
DESCRIPTION = (
    "Your poison or weapon oil's <killing blow> makes its corpse explode, dealing "
    "<50% of your damage to that enemy> plus an <Alchemy bonus: 50 at levels 0-25, "
    "100 at 26-50, 150 at 51-75, 200 at 76+> to enemies within <25 feet (7.6 metres)> "
    "in the killing coating's damage types. Enemies killed by a blast also explode, "
    "up to <1/2/3/4 additional generations> at the same Alchemy tiers. The initial "
    "explosion sets the chain limit. Each corpse explodes once. Your maximum Health "
    "is reduced by <50>."
)


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_SHA, 'Unexpected ESP baseline'
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    replacements = {}
    additions = {}
    for remaining in range(1, 5):
        for damage_type, name in enumerate(('Fire', 'Frost', 'Shock', 'Poison')):
            fid = OWN | (0xF90 + (remaining - 1) * 4 + damage_type)
            assert fid not in before
            _, template = before[OWN | (0xF83 + damage_type * 2)]
            body = b''.join(field(tag, f'BT_CorpseExplosion{name}Chain{remaining}'.encode() + b'\0'
                if tag == b'EDID' else value) for tag, value in fields(template))
            additions[fid] = record(b'SPEL', fid, body)
    header_fields = list(fields(before[0][1]))
    hedr = bytearray(dict(header_fields)[b'HEDR'])
    struct.pack_into('<I', hedr, 4, u32(hedr, 4) + len(additions))
    struct.pack_into('<I', hedr, 8, max(u32(hedr, 8), 0xFA0))
    replacements[0] = b''.join(field(t, bytes(hedr) if t == b'HEDR' else v) for t, v in header_fields)
    for fid, tag in ((OWN | 0xF80, b'DESC'), (OWN | 0xF81, b'DNAM')):
        old_fields = list(fields(before[fid][1]))
        old_text = dict(old_fields)[tag]
        assert old_text.count(b'50%') == 1 and b'No friendly fire or chain reactions.' in old_text
        text = DESCRIPTION if tag == b'DESC' else DESCRIPTION.replace('<', '').replace('>', '')
        replacements[fid] = b''.join(field(t, text.encode() + b'\0' if t == tag else v) for t, v in old_fields)

    spell_groups = 0
    def rewrite(blob):
        nonlocal spell_groups
        out = bytearray(); pos = 0
        while pos < len(blob):
            h = bytearray(blob[pos:pos + 24]); size = u32(h, 4)
            if h[:4] == b'GRUP':
                body = rewrite(blob[pos + 24:pos + size])
                if h[8:12] == b'SPEL' and u32(h, 12) == 0:
                    body += b''.join(additions.values())
                    spell_groups += 1
                struct.pack_into('<I', h, 4, len(body) + 24)
                out += h + body; pos += size
            else:
                fid = u32(h, 12)
                out += record(h[:4], fid, replacements[fid], head=h) if fid in replacements else blob[pos:pos + 24 + size]
                pos += 24 + size
        assert pos == len(blob)
        return bytes(out)

    updated = rewrite(original)
    assert spell_groups == 1
    after = {u32(h, 12): (h, b) for h, b in records(updated)}
    assert set(after) == set(before) | set(additions)
    assert {fid for fid in before if before[fid] != after[fid]} == set(replacements)
    for fid in replacements:
        if fid == 0:
            assert [(t, v) for t, v in fields(before[0][1]) if t != b'HEDR'] == [
                (t, v) for t, v in fields(after[0][1]) if t != b'HEDR']
            continue
        tag = b'DESC' if fid == OWN | 0xF80 else b'DNAM'
        old_fields = list(fields(before[fid][1])); new_fields = list(fields(after[fid][1]))
        assert [(t, v) for t, v in old_fields if t != tag] == [(t, v) for t, v in new_fields if t != tag]
        text = dict(new_fields)[tag]
        assert b'50%' in text and b'Alchemy bonus' in text and b'also explode' in text
        assert all(tier in text for tier in (b'50 at levels 0-25', b'100 at 26-50', b'150 at 51-75', b'200 at 76+'))
        assert b'1/2/3/4 additional generations' in text
    for remaining in range(1, 5):
        for damage_type in range(4):
            fid = OWN | (0xF90 + (remaining - 1) * 4 + damage_type)
            template = before[OWN | (0xF83 + damage_type * 2)][1]
            assert [(t, v) for t, v in fields(template) if t != b'EDID'] == [
                (t, v) for t, v in fields(after[fid][1]) if t != b'EDID']
    return updated
