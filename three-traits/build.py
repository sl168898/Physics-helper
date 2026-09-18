"""Add three traits to the latest combined mod without changing existing records."""
from pathlib import Path
import sys, struct, zipfile, json, hashlib, io
from PIL import Image
sys.path.insert(0, 'venom-harvester/tools')
from esp import records, fields, field, record, group

root = Path('three-traits')
base = Path('outputs/Biggie_Traits_Combined_Single_ESP_v2_3_1.zip')
sha = lambda b: hashlib.sha256(b).hexdigest()
with zipfile.ZipFile(base) as z:
    assert z.testzip() is None
    old = {n: z.read(n) for n in z.namelist()}
files = dict(old)
name = 'Biggie Traits - Combined.esp'
rs = list(records(old[name]))
masters = [v for t, v in fields(rs[0][1]) if t == b'MAST']
own = len(masters) << 24
byid = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in rs}
assert own == 0x21000000
s = lambda t, v: field(t.encode(), v.encode() + b'\0')
u = lambda t, v: field(t.encode(), struct.pack('<I', v))
spit = dict(fields(byid[own | 0x800][1]))[b'SPIT']
marker = dict(fields(byid[own | 0x801][1]))[b'DATA']
src = Path('greybeard-trained/base/Biggie Traits.esp').read_bytes()
carry = bytearray(next(dict(fields(b))[b'DATA'] for h, b in records(src)
                       if dict(fields(b)).get(b'EDID') == b'Traits_Hoarder\0'))
assert len(carry) == 152
assert struct.unpack_from('<II', carry, 64) == (0, 32)
struct.pack_into('<I', carry, 0, 0xA02 | 4)
struct.pack_into('<I', carry, 136, 0)

traits = [
    ('BurdenOfDevotion', 'Burden of Devotion', 0xF00, 50.,
     'While carrying no more than <half your carrying capacity>, shrine blessings you receive last <twice as long> and are <25% stronger>. Your carrying capacity is reduced by <50>.',
     'Shrine blessings received at half carrying capacity or less last twice as long and are 25% stronger. Carrying capacity is reduced by 50.'),
    ('UnbrokenGuard', 'Unbroken Guard', 0xF10, 0.,
     'Blocking <three attacks within 5 seconds> makes your next bash deal <5 times as much damage>. Unblocked physical hits deal <15% more damage> to you.',
     'Block three attacks within 5 seconds to empower your next bash: 5x damage. Unblocked physical hits deal 15% more damage to you.'),
    ('EchoingSteel', 'Echoing Steel', 0xF20, 0.,
     'After shouting, your next melee power attack within <5 seconds> deals <50% more damage>, or <100% more> with a two-handed weapon. Power attacks cost <25% more Stamina>.',
     'After shouting, your next melee power attack within 5 seconds deals 50% more damage, or 100% more with a two-handed weapon. Power attacks cost 25% more Stamina.')
]
new = []
for key, title, local, magnitude, desc, effectdesc in traits:
    abilityid, effectid = own | local, own | (local + 1)
    assert abilityid not in byid and effectid not in byid
    data = bytearray(carry if key == 'BurdenOfDevotion' else marker)
    if key != 'BurdenOfDevotion':
        struct.pack_into('<I', data, 136, own | 0xF22 if key == 'EchoingSteel' else 0)
    effect = s('EDID', 'Traits_' + key) + s('FULL', 'Trait: ' + title) + u('MDOB', 0x435A5)
    effect += field(b'DATA', data) + field(b'SNDD', b'') + s('DNAM', effectdesc)
    ability = s('EDID', 'Traits_' + key + 'Ab') + field(b'OBND', bytes(12)) + s('FULL', title)
    ability += u('ETYP', 0x13F44) + s('DESC', desc) + field(b'SPIT', spit)
    ability += u('EFID', effectid) + field(b'EFIT', struct.pack('<fII', magnitude, 0, 0))
    new += [(b'SPEL', abilityid, ability), (b'MGEF', effectid, effect)]
    files['Biggie Traits - Combined_FLM.ini'] += (
        f'FormList = Traits_AbilityList|Traits_{key}Ab\n'
        f'FormList = Traits_EffectsList|Traits_{key}\n').encode()
    # DXT5 payload with the already working Skyrim/Scaleform DDS header.
    with Image.open(root / 'art' / (key + '.png')) as im:
        im = im.convert('RGBA').resize((1024, 704), Image.Resampling.LANCZOS)
        buf = io.BytesIO()
        im.save(buf, format='DDS', pixel_format='DXT5')
        tex = bytearray(buf.getvalue())
    assert len(tex) == 721024 and tex[84:88] == b'DXT5'
    tex[:128] = old['Interface/TraitPics/Traits_SkaldAb.dds'][:128]
    with Image.open(io.BytesIO(tex)) as im:
        im.load()
        assert im.size == (1024, 704)
    files[f'Interface/TraitPics/Traits_{key}Ab.dds'] = bytes(tex)

# Native Mod Power Attack Stamina entry point (27), Multiply Value (3).
# Structure corroborated with Ordinator's Witchmaster/White Lion cost perks.
perk = s('EDID', 'BT_EchoingSteelStaminaPerk') + s('DESC', '')
perk += field(b'DATA', bytes([0, 0, 1, 1, 0])) + field(b'PRKE', bytes([2, 0, 0]))
perk += field(b'DATA', bytes([27, 3, 2])) + field(b'EPFT', b'\1')
perk += field(b'EPFD', struct.pack('<f', 1.25)) + field(b'PRKF', b'')
new.append((b'PERK', own | 0xF22, perk))
out = {}
for h, b in rs[1:]:
    out.setdefault(h[:4], []).append(record(h[:4], 0, b, head=h))
for tag, fid, payload in new:
    out.setdefault(tag, []).append(record(tag, fid, payload))
header = b''.join(field(t, struct.pack('<fII', 1.7, len(rs) - 1 + len(new), 0xF23) if t == b'HEDR'
                       else b'Ten custom traits in one ESL-flagged ESP.\0' if t == b'SNAM'
                       else v) for t, v in fields(rs[0][1]))
files[name] = record(b'TES4', 0, header, flags=0x200) + b''.join(group(t, b''.join(v)) for t, v in out.items())

# Import only the built helper and its provenance, preserving the old DLL.
with zipfile.ZipFile(root / 'Biggie_Trait_Mechanics_v1.zip') as z:
    assert z.testzip() is None
    native = {n.replace('\\', '/'): z.read(n) for n in z.namelist() if not n.endswith('/')}
info = json.loads(native['BuildInfo.json'].decode('utf-8-sig'))
dll = native['SKSE/Plugins/BiggieTraitMechanics.dll']
assert dll[:2] == b'MZ' and sha(dll) == info['dll_sha256']
assert info['windows_build'] == info['rules_tests'] == 'passed'
for path, digest in info['source_sha256_lf'].items():
    assert sha((root / 'native' / path).read_bytes().replace(b'\r\n', b'\n')) == digest, path
files['SKSE/Plugins/BiggieTraitMechanics.dll'] = dll
for n, b in native.items():
    if not n.endswith('.dll'):
        files['Documentation/ThreeTraits/NativeBuild/' + n] = b
for p in (root / 'native').rglob('*'):
    if p.is_file() and p.suffix in ['.cpp', '.h', '.ps1', '.txt', '.json', '.md']:
        files['Documentation/ThreeTraits/NativeSource/' + str(p.relative_to(root / 'native'))] = p.read_bytes()
files['Documentation/ThreeTraits/build.py'] = Path(__file__).read_bytes()
files['Documentation/ThreeTraits/esp.py'] = Path('venom-harvester/tools/esp.py').read_bytes()
files['Documentation/ThreeTraits/Artwork-Prompts.txt'] = (root / 'Artwork-Prompts.txt').read_bytes()
files['Documentation/ThreeTraits/MinHook-LICENSE.txt'] = (root / 'MinHook-LICENSE.txt').read_bytes()
files['README.txt'] = (root / 'README.txt').read_bytes() + old['README.txt']

# Structural and regression checks on the actual emitted plugin and assets.
cr = list(records(files[name]))
cb = {struct.unpack_from('<I', h, 12)[0]: (h, b) for h, b in cr}
assert len(cr) == len(rs) + 7 and len(cb) == len(cr)
for h, b in rs[1:]:
    assert cb[struct.unpack_from('<I', h, 12)[0]] == (h, b)
assert [v for t, v in fields(cr[0][1]) if t == b'MAST'] == masters
assert struct.unpack('<fII', dict(fields(cr[0][1]))[b'HEDR'])[1:] == (len(cr) - 1, 0xF23)
for key, title, local, magnitude, *_ in traits:
    a = dict(fields(cb[own | local][1]))
    e = dict(fields(cb[own | (local + 1)][1]))
    assert a[b'EFID'] == struct.pack('<I', own | (local + 1))
    assert a[b'EFIT'] == struct.pack('<fII', magnitude, 0, 0)
    assert b'VMAD' not in e
    assert struct.unpack_from('<I', e[b'DATA'], 136)[0] == (own | 0xF22 if key == 'EchoingSteel' else 0)
assert dict(fields(cb[own | 0xF22][1]))[b'EPFD'] == struct.pack('<f', 1.25)
assert files['Biggie Traits - Combined_FLM.ini'].count(b'FormList = Traits_AbilityList|') == 10
assert files['Biggie Traits - Combined_FLM.ini'].count(b'FormList = Traits_EffectsList|') == 10
for n, b in old.items():
    if n not in [name, 'Biggie Traits - Combined_FLM.ini', 'README.txt', 'Validation.json']:
        assert files[n] == b, n
report = {
    'version': '2.4', 'new_records': {hex(fid): tag.decode() for tag, fid, _ in new},
    'previous_records_byte_identical': len(rs) - 1,
    'previous_thumbnails_scripts_and_dlls_byte_identical': True,
    'new_masters': 0, 'new_inventory_items': 0, 'new_scripts': 0,
    'burden': {'capacity_penalty': 50, 'weight_fraction_inclusive': 0.5,
               'capacity_after_penalty': True, 'check_at_blessing_receipt': True,
               'blessing_keyword': 'Skyrim.esm|000FB98C', 'duration_multiplier': 2, 'magnitude_multiplier': 1.25},
    'guard': {'blocks': 3, 'rolling_window_seconds': 5, 'bash_multiplier': 5,
              'one_charge_no_stacking': True, 'unblocked_physical_hit_multiplier': 1.15},
    'echo': {'window_seconds': 5, 'one_handed_multiplier': 1.5, 'two_handed_multiplier': 2,
             'power_attack_stamina_multiplier': 1.25, 'powers_do_not_trigger': True},
    'native_build': info, 'in_game_tested': False
}
files['Documentation/ThreeTraits/Validation.json'] = json.dumps(report, indent=2).encode()
files['Documentation/v2_3_1-Validation.json'] = old['Validation.json']
m = json.loads(old['Validation.json'])
m.update(version='2.4', baseline=base.name, baseline_sha256=sha(base.read_bytes()), validation=report)
m['traits'].extend(t[1] for t in traits)
m['trait_mechanics_dll'] = info
m['files'] = {n: sha(b) for n, b in sorted(files.items()) if n != 'Validation.json'}
files['Validation.json'] = json.dumps(m, indent=2).encode()
output = Path('outputs/Biggie_Traits_Combined_Single_ESP_v2_4.zip')
with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for n, b in sorted(files.items()):
        z.writestr(n, b)
with zipfile.ZipFile(output) as z:
    assert z.testzip() is None and sum(n.endswith('.esp') for n in z.namelist()) == 1
    assert all(sha(z.read(n)) == h for n, h in m['files'].items())
print(output.resolve())
print(json.dumps({'traits': m['traits'], 'previous_records_preserved': len(rs) - 1,
                  'new_records': len(new), 'sha256': sha(output.read_bytes()), 'bytes': output.stat().st_size}, indent=2))
