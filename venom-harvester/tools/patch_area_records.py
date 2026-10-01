"""Convert our four private corpse-damage spells to native Self-area blasts.

Reference: the user-supplied Ordinator Corpse Gas uses OnDying -> Cast(corpse)
and Self-area Health effects with linked EXPL records. No Ordinator forms,
scripts, meshes, textures or master dependency are copied into this plugin.
"""
import hashlib
import struct
from esp import field, fields, records, record

BASELINE_SHA = 'b40dd38a513f9773dce2612b8a59b8857c32e0b3b751c30436084647f47e9003'
OWN = 0x21000000
u32 = lambda b, offset=0: struct.unpack_from('<I', b, offset)[0]


def patch(original):
    assert hashlib.sha256(original).hexdigest() == BASELINE_SHA, 'Unexpected ESP baseline'
    before = {u32(h, 12): (h, b) for h, b in records(original)}
    replacements = {}
    for i in range(4):
        effect, spell, visual = (OWN | (0xF82 + 2*i), OWN | (0xF83 + 2*i), OWN | (0xF8A + i))
        ef = dict(fields(before[effect][1]))
        data = bytearray(ef[b'DATA'])
        assert len(data) == 152 and u32(data) == 0x8A05
        assert u32(data, 64) == 0 and u32(data, 68) == 24 # ValueModifier, Health
        assert u32(data, 80) == 1 and u32(data, 84) == 3 # FireAndForget, TargetActor
        assert u32(data, 76) == 0
        # Self, area allowed, zero-duration EFIT for an instant hit. The corpse
        # is a valid origin while dying/dead; the damage gate excludes it.
        struct.pack_into('<I', data, 0, (u32(data) & ~((1 << 9) | (1 << 11))) | (1 << 28))
        struct.pack_into('<I', data, 76, visual)
        struct.pack_into('<I', data, 84, 0)
        replacements[effect] = b''.join(field(t, bytes(data) if t == b'DATA' else v) for t, v in fields(before[effect][1]))
        sf = dict(fields(before[spell][1]))
        spit = bytearray(sf[b'SPIT'])
        assert len(spit) == 36 and u32(spit, 4) == 0x300001
        assert u32(spit, 20) == 3 and u32(sf[b'EFID']) == effect
        assert struct.unpack('<fII', sf[b'EFIT']) == (1.0, 0, 0)
        # The plugin already enforces LOS from the corpse and all target
        # restrictions. Native resistance/absorption must not apply twice.
        struct.pack_into('<I', spit, 4, 0x3C0001)
        struct.pack_into('<I', spit, 20, 0)
        replacements[spell] = b''.join(field(t,
            bytes(spit) if t == b'SPIT' else struct.pack('<fII', 1.0, 25, 0) if t == b'EFIT' else v)
            for t, v in fields(before[spell][1]))

    def rewrite(blob):
        out = bytearray(); pos = 0
        while pos < len(blob):
            h = bytearray(blob[pos:pos+24]); size = u32(h, 4)
            if h[:4] == b'GRUP':
                body = rewrite(blob[pos+24:pos+size]); struct.pack_into('<I', h, 4, len(body)+24)
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
    assert len(replacements) == 8
    for fid in before:
        if fid not in replacements: assert before[fid] == after[fid], hex(fid)
        else: assert after[fid][1] == replacements[fid]
    for i in range(4):
        ef = dict(fields(after[OWN | (0xF82+2*i)][1]))
        sf = dict(fields(after[OWN | (0xF83+2*i)][1]))
        ex = dict(fields(after[OWN | (0xF8A+i)][1]))
        assert u32(ef[b'DATA'], 84) == u32(sf[b'SPIT'], 20) == 0
        assert u32(ef[b'DATA'], 76) == OWN | (0xF8A+i)
        assert u32(ef[b'DATA'], 16) == (41,43,42,40)[i]
        assert not u32(ef[b'DATA']) & ((1 << 1) | (1 << 11) | (1 << 21)) # no Recover/NoArea/second scaling
        assert u32(sf[b'SPIT'], 4) == 0x3C0001
        assert struct.unpack('<fII', sf[b'EFIT']) == (1.0,25,0)
        assert struct.unpack_from('<ff', ex[b'DATA'], 24) == (0,0) # presentation has no extra damage/force
    return updated
