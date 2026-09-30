"""Convert Combined 2.10.12 Voice of Authority to an independent Speech perk.

Usage: python build_voice_perk.py BASELINE_DIRECTORY OUTPUT_DIRECTORY
The baseline is never modified. No game masters or external binary tools required.
"""
from pathlib import Path
import collections
import hashlib
import json
import math
import struct
import sys
import zlib

PLUGIN = 'Biggie Traits - Combined.esp'
FLM = 'Biggie Traits - Combined_FLM.ini'
CONFIG = 'SKSE/Plugins/PerkAdjuster/VoiceOfAuthority.json'
NEW_ID = 0x21000807
U32 = lambda b: struct.unpack('<I', b)[0]


def fields(body):
    pos = 0
    while pos < len(body):
        assert pos + 6 <= len(body)
        tag, size = struct.unpack_from('<4sH', body, pos)
        assert tag != b'XXXX', 'Unexpected extended subrecord in pinned input'
        value = body[pos + 6:pos + 6 + size]
        assert len(value) == size
        yield tag, value
        pos += size + 6
    assert pos == len(body)


def pack(fs):
    return b''.join(struct.pack('<4sH', tag, len(value)) + value for tag, value in fs)


def records(blob):
    pos = 0
    while pos < len(blob):
        head = blob[pos:pos + 24]
        assert len(head) == 24
        size = U32(head[4:8])
        if head[:4] == b'GRUP':
            assert size >= 24 and pos + size <= len(blob)
            yield from records(blob[pos + 24:pos + size])
            pos += size
        else:
            body = blob[pos + 24:pos + 24 + size]
            assert len(body) == size
            if U32(head[8:12]) & 0x40000:
                decoded = zlib.decompress(body[4:])
                assert len(decoded) == U32(body[:4])
                body = decoded
            yield U32(head[12:16]), (head, body)
            pos += size + 24
    assert pos == len(blob)


def rewrite(blob, replacements, addition, counts):
    out = bytearray()
    pos = 0
    while pos < len(blob):
        head = bytearray(blob[pos:pos + 24])
        size = U32(head[4:8])
        if head[:4] == b'GRUP':
            counts['groups'] += 1
            body = rewrite(blob[pos + 24:pos + size], replacements, addition, counts)
            if head[8:12] == b'PERK' and U32(head[12:16]) == 0:
                counts['insertions'] += 1
                body += addition
            struct.pack_into('<I', head, 4, len(body) + 24)
            out += head + body
            pos += size
        else:
            form = U32(head[12:16])
            if form in replacements:
                body = replacements[form]
                struct.pack_into('<I', head, 4, len(body))
                struct.pack_into('<I', head, 8, U32(head[8:12]) & ~0x40000)
                out += head + body
            else:
                out += blob[pos:pos + 24 + size]
            pos += size + 24
    assert pos == len(blob)
    return bytes(out)


def patch_esp(original):
    before = dict(records(original))
    assert len(before) == 77 and NEW_ID not in before
    assert before[0][0][:4] == b'TES4' and U32(before[0][0][8:12]) & 0x200
    master_fields = [(t, v) for t, v in fields(before[0][1]) if t == b'MAST']
    assert len(master_fields) == 33
    assert before[0x21000802][0][:4] == b'PERK'
    old_perk = list(fields(before[0x21000802][1]))
    start = next(i for i, (t, v) in enumerate(old_perk) if t == b'PRKE')
    entry = old_perk[start:]
    assert [t for t, v in entry] == [b'PRKE', b'DATA', b'PRKC', b'CTDA', b'EPFT', b'EPFD', b'PRKF']
    assert dict(entry)[b'DATA'] == bytes([29, 14, 3])
    assert dict(entry)[b'PRKC'] == b'\x01'
    assert dict(entry)[b'CTDA'].hex() == '000000000000803fb5020000996b0400000000000000000000000000ffffffff'
    av, factor = struct.unpack('<ff', dict(entry)[b'EPFD'])
    assert av == 17 and math.isclose(factor, 0.01, rel_tol=1e-6)
    # GetBaseActorValue Speechcraft >= 10; no parent perk or other prerequisite.
    speech_condition = struct.pack('<B3sfH2sIIIIi', 0x60, b'\0'*3, 10.0, 277, b'\0'*2, 17, 0, 0, 0, -1)
    new_fields = [
        (b'EDID', b'BT_VoiceOfAuthoritySpeechPerk\0'),
        (b'FULL', b'Voice of Authority\0'),
        (b'DESC', b'Your shouts are 1% stronger per point of current Speech, including bonuses, with no cap.\0'),
        (b'CTDA', speech_condition),
        (b'DATA', bytes([0, 1, 1, 1, 0])),  # ordinary perk, level 1, one rank, playable, visible
        *entry,
    ]
    new_body = pack(new_fields)
    new_record = struct.pack('<4sIIIIHH', b'PERK', len(new_body), 0, NEW_ID, 0, 44, 0) + new_body

    replacements = {}
    # Retain the legacy identity for safe saved-game cleanup; it no longer boosts shouts.
    replacements[0x21000802] = pack([
        (t, bytes([0, 0, 1, 0, 1]) if t == b'DATA' else v)
        for t, v in old_perk[:start]
    ])
    old_effect = list(fields(before[0x21000801][1]))
    assert sum(t == b'VMAD' for t, v in old_effect) == 1
    replacements[0x21000801] = pack([
        (t, b'Retired trait. Voice of Authority is now available in the Speech perk tree.\0' if t == b'DNAM' else v)
        for t, v in old_effect if t != b'VMAD'
    ])
    replacements[0x21000800] = pack([
        (t, b'Voice of Authority is now a Speech perk requiring Speech 10. This legacy trait is removed when a game loads.\0' if t == b'DESC' else v)
        for t, v in fields(before[0x21000800][1])
    ])
    header = []
    for t, v in fields(before[0][1]):
        if t == b'HEDR':
            v = bytearray(v)
            struct.pack_into('<I', v, 4, U32(v[4:8]) + 1)
            v = bytes(v)
        elif t == b'SNAM':
            v = b'Twelve custom traits and the Voice of Authority Speech perk.\0'
        header.append((t, v))
    replacements[0] = pack(header)
    counts = collections.Counter()
    updated = rewrite(original, replacements, new_record, counts)
    assert counts['insertions'] == 1
    after = dict(records(updated))
    assert set(after) == set(before) | {NEW_ID}
    assert after[NEW_ID][1] == new_body
    for form, (head, body) in before.items():
        nh, nb = after[form]
        if form not in replacements:
            assert (nh, nb) == (head, body), hex(form)
        else:
            assert nh[:4] == head[:4] and nh[8:] == head[8:]
            assert nb == replacements[form]
    assert [(t, v) for t, v in fields(after[0][1]) if t not in (b'HEDR', b'SNAM')] == [(t, v) for t, v in fields(before[0][1]) if t not in (b'HEDR', b'SNAM')]
    old_hedr = dict(fields(before[0][1]))[b'HEDR']
    new_hedr = dict(fields(after[0][1]))[b'HEDR']
    assert old_hedr[:4] == new_hedr[:4] and old_hedr[8:] == new_hedr[8:]
    assert U32(new_hedr[4:8]) == len(after) - 1 + counts['groups']
    # Preserve the exact cooldown effect definition so saved modifiers reverse normally.
    assert after[0x21000806] == before[0x21000806]
    assert [(t, v) for t, v in fields(after[0x21000801][1]) if t != b'DNAM'] == [(t, v) for t, v in old_effect if t not in (b'VMAD', b'DNAM')]
    assert [(t, v) for t, v in fields(after[0x21000800][1]) if t != b'DESC'] == [(t, v) for t, v in fields(before[0x21000800][1]) if t != b'DESC']
    assert not any(t == b'PRKE' for t, v in fields(after[0x21000802][1]))
    assert new_fields[5:] == entry  # identical uncapped current-Speech multiplier and shout filter
    formula_cases = []
    for speech, expected in [(0, 1), (10, 1.1), (50, 1.5), (70.5, 1.705), (100, 2), (150, 2.5), (250, 3.5)]:
        actual = 1 + speech * factor
        assert math.isclose(actual, expected, rel_tol=1e-6)
        formula_cases.append({'current_speech': speech, 'multiplier': round(actual, 6)})
    return updated, {
        'status': 'PASS', 'version': '2.11.0-beta1', 'baseline': '2.10.12-beta1',
        'baseline_esp_sha256': hashlib.sha256(original).hexdigest(),
        'esp_sha256': hashlib.sha256(updated).hexdigest(),
        'changed_existing_forms': [f'{x:08X}' for x in replacements],
        'added_perk': '0x807|Biggie Traits - Combined.esp',
        'speech_actor_value_record': '0x457|Skyrim.esm',
        'rank_count': 1, 'normal_perk_point_cost': 1, 'required_base_speech': 10,
        'other_prerequisites': [], 'granted_shouts': [], 'drawbacks': [],
        'formula': '1 + current Speech / 100; no cap', 'formula_cases': formula_cases,
        'native_multiplier_and_shout_filter_identical': True,
        'all_other_records_byte_identical': True, 'masters_and_existing_form_ids_preserved': True,
        'esl_flag_preserved': True, 'legacy_cooldown_definition_preserved_for_dispel': True,
        'legacy_effect_script_detached': True, 'legacy_perk_entries_removed': True,
        'new_perk_has_no_script_or_ability_grant': True, 'avif_overrides': 0,
        'record_count': len(after) - 1, 'group_count': counts['groups'],
        'in_game_tested': False,
    }


def main(source, output):
    esp, report = patch_esp((source / PLUGIN).read_bytes())
    flm = (source / FLM).read_text()
    old_lines = flm.splitlines(keepends=True)
    removed = {
        '; Append a matched ability/effect pair for both the selection and removal menus.',
        'FormList = Traits_AbilityList|Traits_GreybeardTrainedAb',
        'FormList = Traits_EffectsList|Traits_GreybeardTrained',
    }
    assert {s.strip() for s in old_lines} >= removed
    new_lines = [s for s in old_lines if s.strip() not in removed]
    flm = ''.join(new_lines)
    assert 'GreybeardTrained' not in flm
    abilities = [s for s in new_lines if s.startswith('FormList = Traits_AbilityList|')]
    effects = [s for s in new_lines if s.startswith('FormList = Traits_EffectsList|')]
    assert len(abilities) == len(effects) == 12
    report['remaining_trait_pairs'] = 12
    config = {'additions': [{
        'perk': '0x807|Biggie Traits - Combined.esp',
        'skill': '0x457|Skyrim.esm', 'x': 4.8, 'y': 0.75,
    }]}
    output.mkdir(parents=True, exist_ok=True)
    (output / PLUGIN).write_bytes(esp)
    (output / FLM).write_text(flm)
    (output / CONFIG).parent.mkdir(parents=True, exist_ok=True)
    (output / CONFIG).write_text(json.dumps(config, indent=2) + '\n')
    (output / 'Voice-Perk-Validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main(Path(sys.argv[1]), Path(sys.argv[2]))
