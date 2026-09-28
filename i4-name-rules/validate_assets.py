"""Independent binary and configuration checks for the distributable assets."""
from pathlib import Path
import argparse
import json
import struct
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
STAGE = ROOT / 'stage'


class Reader:
    def __init__(self, data, pos=0):
        self.data, self.bit = data, pos * 8

    def read(self, count, signed=False):
        value = 0
        for _ in range(count):
            value = (value << 1) | ((self.data[self.bit // 8] >> (7-self.bit % 8)) & 1)
            self.bit += 1
        return value - (1 << count) if signed and value & (1 << (count-1)) else value

    def align(self): self.bit = (self.bit+7)//8*8
    @property
    def pos(self): return (self.bit+7)//8


def read_rect(reader):
    width = reader.read(5)
    result = tuple(reader.read(width, signed=True) for _ in range(4))
    reader.align()
    return result


def tags(data, pos):
    while pos < len(data):
        header, = struct.unpack_from('<H',data,pos)
        pos += 2
        code, size = header >> 6, header & 63
        if size == 63:
            size, = struct.unpack_from('<I',data,pos)
            pos += 4
        assert pos+size <= len(data), 'Truncated SWF tag'
        body = data[pos:pos+size]
        pos += size
        yield code, body
        if code == 0:
            assert pos == len(data), 'Unexpected bytes after End tag'
            return
    raise AssertionError('Missing SWF End tag')


def decode_shape(data, rgba=False):
    character, = struct.unpack_from('<H',data)
    r = Reader(data,2)
    bounds = read_rect(r)
    assert r.read(8) == 1 and r.read(8) == 0  # one solid fill
    color = tuple(r.read(8) for _ in range(4 if rgba else 3))
    assert r.read(8) == 0  # no lines
    fill_bits, line_bits = r.read(4), r.read(4)
    x = y = 0
    polygons = []
    fill0 = fill1 = 0
    while True:
        if r.read(1):
            assert r.read(1) == 1, 'Expected straight vector edges'
            n = r.read(4)+2
            if r.read(1):
                dx,dy = r.read(n,True),r.read(n,True)
            elif r.read(1): dx,dy = 0,r.read(n,True)
            else: dx,dy = r.read(n,True),0
            x,y = x+dx,y+dy
            assert fill0 == 0 and fill1 == 1
            polygons[-1].append((x,y))
        else:
            flags = r.read(5)
            if flags == 0: break
            assert not flags & 0x18
            if flags & 1:
                n = r.read(5)
                x,y = r.read(n,True),r.read(n,True)
                polygons.append([(x,y)])
            if flags & 2: fill0 = r.read(fill_bits)
            if flags & 4: fill1 = r.read(fill_bits)
    assert r.pos == len(data), 'Trailing shape bytes'
    assert all(p[0] == p[-1] for p in polygons), 'Open contour'
    return character,bounds,color,polygons


def validate():
    report = {}
    esp = (STAGE/'RenamePotions_I4.esp').read_bytes()
    sig,size,flags,formid,_,version,_ = struct.unpack_from('<4sIIIIHH',esp)
    assert (sig,flags,formid,version) == (b'TES4',0x200,0,44)
    assert size+24 == len(esp)
    pos, subs = 24, []
    while pos < len(esp):
        kind,length = struct.unpack_from('<4sH',esp,pos)
        pos += 6
        subs.append((kind,esp[pos:pos+length])); pos += length
    assert pos == len(esp)
    assert struct.unpack('<fII',dict(subs)[b'HEDR'])[1:] == (0,0x800)
    masters = [data.rstrip(b'\0').decode() for kind,data in subs if kind==b'MAST']
    assert masters == ['Skyrim.esm','I4IconAddon.esp']
    report['plugin'] = {'esl':True,'gameplayRecords':0,'masters':masters}

    data = (STAGE/'Interface/RenamePotionsI4/icons.swf').read_bytes()
    assert data[:4] == b'FWS\x0a'
    assert struct.unpack_from('<I',data,4)[0] == len(data)
    r = Reader(data,8)
    assert read_rect(r) == (0,2560,0,2560)
    rate,frames = struct.unpack_from('<HH',data,r.pos)
    assert (rate,frames) == (24*256,2)
    records = list(tags(data,r.pos+4))
    assert sum(c==1 for c,b in records) == 2
    labels = [b.rstrip(b'\0').decode() for c,b in records if c==43]
    assert labels == ['weapon_oil']
    assert next(b for c,b in records if c==12) == b'\x07\x00'
    transparent = decode_shape(next(b for c,b in records if c==32), True)
    assert transparent[0:3] == (1,(0,2560,0,2560),(255,255,255,0))
    artwork = decode_shape(next(b for c,b in records if c==2))
    assert artwork[0] == 2 and artwork[2] == (255,255,255)
    sprite = next(b for c,b in records if c==39)
    assert struct.unpack_from('<HH',sprite) == (3,1)
    sprite_tags = list(tags(sprite,4))
    assert sprite_tags == [(26,struct.pack('<BHHB',6,1,2,0)),(1,b''),(0,b'')]
    placed = [b for c,b in records if c==26]
    assert placed == [struct.pack('<BHHB',6,3,1,0),struct.pack('<BHHB',0x26,4,3,0)+b'icon\0']
    assert [c for c,b in records].index(43) > [c for c,b in records].index(1)
    svg = ET.parse(STAGE/'Interface/RenamePotionsI4/weapon_oil.svg').getroot()
    assert svg.attrib['width'] == svg.attrib['height'] == '128'
    group = svg.find('{http://www.w3.org/2000/svg}g')
    assert group.attrib['fill'] == '#FFFFFF'
    svg_polys = [[tuple(int(n)*20 for n in point.split(',')) for point in p.attrib['points'].split()] for p in group]
    assert [p[:-1] for p in artwork[3]] == svg_polys, 'Inventory and Wheeler icons differ'
    report['artwork'] = {'swfVersion':10,'frames':2,'labels':labels,'closedPolygons':len(svg_polys),
                         'whiteTintableMovieClip':True,'svgMatchesSwf':True}

    config = json.loads((STAGE/'SKSE/Plugins/InventoryInjector/RenamePotions_I4.json').read_text())
    seen = set()
    for index, rule in enumerate(config['rules']):
        match = rule['match']
        assert match['formType'] == 'Potion'
        oil = rule['assign']['iconSource'] == 'RenamePotionsI4/icons.swf'
        expected_keys = {'iconSource','iconLabel','iconColor'}
        if oil:
            assert match['flags'] == ['Poison']
            assert 'formId' not in match
            assert rule['assign']['subTypeDisplay'] == 'Weapon Oil'
            expected_keys.add('subTypeDisplay')
        else:
            assert match['formId'] == {'min':0xFF000000,'max':0xFFFFFFFF}
        assert set(rule['assign']) == expected_keys
        names = match['text']['anyOf']
        if index == 0:
            assert match['text'] == {'contains':'weapon oil','ignoreCase':True,'anyOf':[]}
            assert oil
        else:
            assert names and all(isinstance(n,str) and n for n in names)
            assert len(names) == len(set(names)) and not seen.intersection(names)
            seen.update(names)
        color = rule['assign']['iconColor']
        assert len(color) == 7 and color[0] == '#'; int(color[1:],16)
        if oil: assert rule['assign']['iconLabel'] in labels

    # Configuration contract checks; C++ tests exercise the production name matcher.
    def resolve(name, formid=0xFF001234, formtype='Potion', poison=True, keyword_dll=True):
        result = {}
        for rule in config['rules']:
            match = rule['match']
            if match['formType'] != formtype: continue
            if 'flags' in match and not poison: continue
            if 'formId' in match and not match['formId']['min'] <= formid <= match['formId']['max']: continue
            spec = match['text']
            if keyword_dll and 'contains' in spec:
                found = spec['contains'] in name.lower()
            else:
                found = name in spec['anyOf']
            if found: result.update(rule['assign'])
        return result

    checks = 0
    def check(value):
        nonlocal checks
        assert value
        checks += 1
    for name in ['Weapon Oil of Jarrin Crown', 'WEAPON OIL OF JARRIN CROWN',
                 'Greater wEaPoN oIl of Jarrin Crown III', 'Jarrin Crown weapon oil',
                 'Weapon Oil', 'weapon oils', 'Strong Weapon Oil', 'Weapon Oil II',
                 'Weapon Oil (3)']:
        result = resolve(name)
        check(result.get('iconLabel') == 'weapon_oil' and result.get('subTypeDisplay') == 'Weapon Oil')
    check(resolve('Weapon Oil of Jarrin Crown', 0x00001234).get('iconLabel') == 'weapon_oil')
    check(not resolve('Weapon Oil of Jarrin Crown', poison=False))
    check(not resolve('Weapon Oil', poison=False))
    check(not resolve('Fire Oil', poison=False))
    check(not resolve('Weapon Oil of Jarrin Crown', formtype='Weapon'))
    check(not resolve('Poison of Weakness to Fire'))
    check(not resolve('Potion of Restore Health (Remarkable)', poison=False))
    check(not resolve('Unlisted Custom Name'))
    check(not resolve('WeaponOil of Jarrin Crown'))
    check(not resolve('Weapon-Oil of Jarrin Crown'))
    for name, color in [('Fire Oil','#E85B39'),('Frost Oil','#7DDBEF'),('Shock Oil','#EAD66A'),
                        ('Fire Weapon Oil','#E85B39'),('Weapon Oil - Frost','#7DDBEF')]:
        check(resolve(name).get('iconColor') == color)
    check(resolve('Healing Potion', poison=False).get('iconLabel') == 'potion_health')
    check(not resolve('Healing Potion', 0x00001234, poison=False))
    check(not resolve('Weapon Oil of Jarrin Crown', keyword_dll=False))
    check(not resolve('Unrelated Poison', keyword_dll=False))
    check(resolve('Weapon Oil', keyword_dll=False).get('iconLabel') == 'weapon_oil')
    report['rules'] = {'count':len(config['rules']),'uniqueExactNames':len(seen),
                       'configurationChecks':checks,'keyword':'weapon oil','caseInsensitive':True,
                       'oilRequiresPoison':True,'oilIncludesStaticForms':True,
                       'stockI4KeywordFallback':'no match','typeLabel':'Weapon Oil'}
    (ROOT/'asset-validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))
    return artwork[3]


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage',type=Path,default=STAGE)
    STAGE=parser.parse_args().stage
    validate()
