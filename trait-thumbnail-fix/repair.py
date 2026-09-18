"""Repair DDS metadata, keeping the original BC3 image bytes exactly intact."""
from pathlib import Path
from io import BytesIO
import hashlib
import json
import struct
import zipfile
from PIL import Image

ROOT = Path(__file__).resolve().parent
BASE = ROOT / 'input/Biggie_Traits_Combined_Single_ESP_v2_1.zip'
TARGETS = {
    'Traits_SkaldAb.dds': 'Skald',
    'Traits_DevotedAlchemistAb.dds': 'Venom Harvester',
    'Traits_GreybeardTrainedAb.dds': 'Voice of Authority',
}
sha = lambda b: hashlib.sha256(b).hexdigest()

def inspect(data):
    assert data[:4] == b'DDS ' and len(data) >= 128
    assert struct.unpack_from('<I', data, 4)[0] == 124
    assert struct.unpack_from('<I', data, 76)[0] == 32
    assert struct.unpack_from('<I', data, 80)[0] == 4 and data[84:88] == b'DXT5'
    flags, height, width, linear, depth, mips = struct.unpack_from('<6I', data, 8)
    assert (width, height) == (1024, 704) and depth == 0 and mips in (0, 1)
    assert flags == 0x81007 and struct.unpack_from('<I', data, 108)[0] == 0x1000
    expected = ((width + 3) // 4) * ((height + 3) // 4) * 16
    assert len(data) == 128 + expected
    rgb_bits = struct.unpack_from('<I', data, 88)[0]
    assert data[92:108] == bytes(16)
    errors = []
    if linear != expected: errors.append('linear_size')
    if rgb_bits != 0: errors.append('rgb_bit_count')
    return {'width': width, 'height': height, 'fourcc': 'DXT5',
            'linear_size': linear, 'required_linear_size': expected,
            'rgb_bit_count': rgb_bits, 'errors': errors}

def repair(data):
    old = inspect(data)
    fixed = bytearray(data)
    struct.pack_into('<I', fixed, 20, old['required_linear_size'])
    struct.pack_into('<I', fixed, 88, 0)
    fixed = bytes(fixed)
    assert inspect(fixed)['errors'] == []
    assert fixed[128:] == data[128:]  # never re-encode, crop or alter artwork
    assert {i for i, (a, b) in enumerate(zip(data, fixed)) if a != b} <= {20, 21, 22, 23, 88, 89, 90, 91}
    return fixed

def main():
    assert sha(BASE.read_bytes()) == 'e36454dca5cee4a8ee3bea86c0b62de7b3d5a5608fa75f5d3f4539edf6d2a198'
    reference = (ROOT.parent / 'biggie-thumbnails/reference/Interface/TraitPics/Traits_AddictAb.dds').read_bytes()
    assert inspect(reference)['errors'] == []
    with zipfile.ZipFile(BASE) as z:
        assert z.testzip() is None
        files = {n: z.read(n) for n in z.namelist() if not n.endswith('/')}
    report = {'baseline': BASE.name, 'baseline_sha256': sha(BASE.read_bytes()),
              'repair_kind': 'DDS header only; original compressed image payload preserved',
              'reference_header_sha256': sha(reference[:128]),
              'sources': ['https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-header',
                          'https://learn.microsoft.com/en-us/windows/win32/direct3ddds/dds-pixelformat'],
              'textures': [], 'in_game_tested': False}
    for name, original in files.items():
        if not name.endswith('.dds'): continue
        meta = inspect(original)
        if Path(name).name not in TARGETS:
            assert not meta['errors'], name
            continue
        assert set(meta['errors']) == {'linear_size', 'rgb_bit_count'}, name
        fixed = repair(original)
        assert fixed[:128] == reference[:128], 'Must match the original mod header exactly'
        assert repair(fixed) == fixed, 'Repair must be idempotent'
        before = Image.open(BytesIO(original)).convert('RGBA')
        after = Image.open(BytesIO(fixed)).convert('RGBA')
        assert before.size == after.size == (1024, 704)
        assert before.tobytes() == after.tobytes(), 'Decoded pixels must stay identical'
        output = ROOT / 'overlay' / name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(fixed)
        report['textures'].append({'file': name, 'trait': TARGETS[Path(name).name],
            'before': meta, 'after': inspect(fixed),
            'old_sha256': sha(original), 'new_sha256': sha(fixed),
            'image_payload_sha256': sha(fixed[128:]), 'image_payload_identical': True,
            'decoded_pixels_identical': True, 'header_matches_original_mod': True,
            'changed_byte_offsets': [i for i, (a, b) in enumerate(zip(original, fixed)) if a != b]})
    assert len(report['textures']) == 3
    (ROOT / 'validation.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__': main()
