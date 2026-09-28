"""Build a display-name I4 configuration and a small original vector icon."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zipfile

ROOT = Path(__file__).resolve().parent
STAGE = ROOT / 'stage'
PLUGIN = 'RenamePotions_I4'
ICON_SOURCE = 'RenamePotionsI4/icons.swf'
SKYUI_SOURCE = 'skyui/icons_item_psychosteve.swf'

# Original artwork. Coordinates are in the same 128 x 128 space as I4 icons.
OIL_POLYGONS = [
    [(22, 76), (61, 24), (79, 10), (76, 33), (38, 87)],
    [(12, 75), (19, 65), (51, 87), (44, 96)],
    [(19, 86), (30, 94), (16, 113), (5, 106)],
    [(4, 109), (14, 116), (10, 122), (0, 115)],
    [(96, 46), (87, 61), (80, 72), (76, 84), (77, 96),
     (82, 106), (89, 113), (98, 116), (108, 114), (116, 108),
     (121, 99), (123, 89), (121, 78), (115, 68), (105, 54)],
]

PROFILES = [
    ('Weapon oil', 'weapon_oil', '#E4B45D', ICON_SOURCE,
     ['Weapon Oil', 'Weapon Oils', 'Blade Oil', 'Sword Oil', 'Bow Oil', 'Oil of Sharpness']),
    ('Fire oil', 'weapon_oil', '#E85B39', ICON_SOURCE,
     ['Fire Oil', 'Flame Oil', 'Burning Oil', 'Fire Weapon Oil', 'Weapon Oil - Fire']),
    ('Frost oil', 'weapon_oil', '#7DDBEF', ICON_SOURCE,
     ['Frost Oil', 'Ice Oil', 'Freezing Oil', 'Frost Weapon Oil', 'Weapon Oil - Frost']),
    ('Shock oil', 'weapon_oil', '#EAD66A', ICON_SOURCE,
     ['Shock Oil', 'Lightning Oil', 'Storm Oil', 'Shock Weapon Oil', 'Weapon Oil - Shock']),
    ('Health', 'potion_health', '#DB2E73', SKYUI_SOURCE,
     ['Healing Potion', 'Health Potion', 'Healing Draught', 'Healing Elixir', 'Potion of Healing']),
    ('Magicka', 'potion_magic', '#2E9FDB', SKYUI_SOURCE,
     ['Magicka Potion', 'Mana Potion', 'Magicka Draught', 'Magicka Elixir', 'Potion of Magicka']),
    ('Stamina', 'potion_stam', '#51DB2E', SKYUI_SOURCE,
     ['Stamina Potion', 'Stamina Draught', 'Stamina Elixir', 'Potion of Stamina']),
    ('Poison', 'potion_poison', '#AD00B3', SKYUI_SOURCE,
     ['Poison', 'Venom', 'Toxin', 'Weapon Poison', 'Blade Poison']),
    ('Fire resistance', 'potion_fire', '#C73636', SKYUI_SOURCE,
     ['Fire Resistance', 'Fire Resistance Potion', 'Potion of Fire Resistance']),
    ('Frost resistance', 'potion_frost', '#1FFBFF', SKYUI_SOURCE,
     ['Frost Resistance', 'Frost Resistance Potion', 'Potion of Frost Resistance']),
    ('Shock resistance', 'potion_shock', '#EAAB00', SKYUI_SOURCE,
     ['Shock Resistance', 'Shock Resistance Potion', 'Potion of Shock Resistance']),
]


class Bits:
    def __init__(self): self.data = []

    def put(self, value, width):
        assert width >= 0 and -(1 << width) <= value < (1 << width)
        self.data.extend((value >> i) & 1 for i in reversed(range(width)))

    def finish(self):
        self.data += [0] * (-len(self.data) % 8)
        return bytes(sum(self.data[i+j] << (7-j) for j in range(8))
                     for i in range(0, len(self.data), 8))


def signed_width(values):
    return max(2, max((v.bit_length() + 1 if v >= 0 else (~v).bit_length() + 1)
                      for v in values))


def rect(values):
    bits = Bits()
    width = signed_width(values)
    bits.put(width, 5)
    for value in values: bits.put(value, width)
    return bits.finish()


def tag(code, body=b''):
    if len(body) < 63: return struct.pack('<H', code << 6 | len(body)) + body
    return struct.pack('<HI', code << 6 | 63, len(body)) + body


def shape(shape_id, polygons, transparent=False):
    polygons = [[(int(x*20), int(y*20)) for x,y in points] for points in polygons]
    points = [p for polygon in polygons for p in polygon]
    bounds = (min(p[0] for p in points), max(p[0] for p in points),
              min(p[1] for p in points), max(p[1] for p in points))
    edges = Bits()
    edges.put(1, 4)  # One fill style bit.
    edges.put(0, 4)  # No line styles.
    for polygon in polygons:
        # MoveTo + FillStyle1, then closed straight-edge contours.
        edges.put(0, 1)
        edges.put(0b00101, 5)
        width = signed_width(polygon[0])
        edges.put(width, 5)
        edges.put(polygon[0][0], width)
        edges.put(polygon[0][1], width)
        edges.put(1, 1)
        for (x,y), (nx,ny) in zip(polygon, polygon[1:] + polygon[:1]):
            dx,dy = nx-x,ny-y
            width = signed_width([dx,dy])
            assert width <= 17
            edges.put(1, 1)
            edges.put(1, 1)
            edges.put(width-2, 4)
            edges.put(1, 1)
            edges.put(dx, width)
            edges.put(dy, width)
    edges.put(0, 6)
    fill = b'\x01\x00\xff\xff\xff' + (b'\x00' if transparent else b'')
    body = struct.pack('<H', shape_id) + rect(bounds) + fill + b'\x00' + edges.finish()
    return tag(32 if transparent else 2, body)


def place(character_id, depth, name=None):
    flags = 0x06 | (0x20 if name else 0)
    body = struct.pack('<BHH', flags, depth, character_id) + b'\x00'  # Identity matrix.
    if name: body += name.encode() + b'\x00'
    return tag(26, body)


def build_swf():
    # A transparent raw shape keeps stable 128px bounds. The artwork is a
    # named child MovieClip so SkyUI/I4 can tint it without changing alpha.
    stream = tag(69, bytes(4)) + tag(9, bytes(3)) + tag(12, b'\x07\x00')
    stream += shape(1, [[(0,0),(128,0),(128,128),(0,128)]], transparent=True)
    stream += place(1, 3) + tag(1)
    stream += tag(43, b'weapon_oil\x00')
    stream += shape(2, OIL_POLYGONS)
    sprite = struct.pack('<HH', 3, 1) + place(2, 1) + tag(1) + tag(0)
    stream += tag(39, sprite) + place(3, 4, 'icon') + tag(1) + tag(0)
    body = rect((0,2560,0,2560)) + struct.pack('<HH', 24*256, 2) + stream
    return b'FWS\x0a' + struct.pack('<I', len(body)+8) + body


def subrecord(name, data): return name.encode() + struct.pack('<H',len(data)) + data


def build_esp():
    body = subrecord('HEDR', struct.pack('<fII', 1.7, 0, 0x800))
    body += subrecord('CNAM', b'Potion name icon compatibility patch\x00')
    body += subrecord('SNAM', b'Loads matching I4 configuration; contains no gameplay records.\x00')
    for master in ['Skyrim.esm', 'I4IconAddon.esp']:
        body += subrecord('MAST', master.encode()+b'\x00')
        body += subrecord('DATA', bytes(8))
    return struct.pack('<4sIIIIHH', b'TES4',len(body),0x200,0,0,44,0) + body


def names_for(bases):
    variants = set()
    for base in bases:
        names = [base]
        names += [prefix+' '+base for prefix in
                  ['Weak','Minor','Lesser','Strong','Greater','Potent','Deadly','Concentrated']]
        names += [base+' '+rank for rank in ['I','II','III','IV','V','1','2','3','4','5']]
        for name in names:
            variants.update([name, name.lower(), name.upper(), name.capitalize()])
    return sorted(variants)


def build_rules():
    rules = [{
        '$comment': 'Weapon oil keyword: any poison whose displayed name contains weapon oil (case-insensitive). Keep empty anyOf so stock I4 matches nothing.',
        'match': {'formType':'Potion', 'flags':['Poison'],
                  'text': {'contains':'weapon oil', 'ignoreCase':True, 'anyOf':[]}},
        'assign': {'iconSource':ICON_SOURCE, 'iconLabel':'weapon_oil',
                   'iconColor':'#E4B45D', 'subTypeDisplay':'Weapon Oil'},
    }]
    all_names = set()
    for title,label,color,source,bases in PROFILES:
        names = names_for(bases)
        assert not all_names.intersection(names), title
        all_names.update(names)
        rule = {
            '$comment': title + ': add exact custom display names to text.anyOf.',
            'match': {'formType':'Potion',
                      'formId': {'min':0xFF000000, 'max':0xFFFFFFFF},
                      'text': {'anyOf':names}},
            'assign': {'iconSource':source,'iconLabel':label,'iconColor':color},
        }
        if source == ICON_SOURCE:
            rule['match'].pop('formId')
            rule['match']['flags'] = ['Poison']
            rule['assign']['subTypeDisplay'] = 'Weapon Oil'
        rules.append(rule)
    return {'$schema':'https://raw.githubusercontent.com/Exit-9B/InventoryInjector/main/docs/InventoryInjector.schema.json',
            '$comment':'v1.3: poison names containing weapon oil use a case-insensitive substring rule with the included modified I4 DLL. Existing exact-name profiles are retained; oil profiles require Poison. Gameplay is unchanged.',
            'rules':rules}


def svg(color='#E4B45D'):
    shapes=''.join('<polygon points="'+ ' '.join(f'{x},{y}' for x,y in points)+'"/>'
                   for points in OIL_POLYGONS)
    return '<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128"><g fill="'+color+'">'+shapes+'</g></svg>\n'


def write(name, data):
    target=STAGE/name
    target.parent.mkdir(parents=True,exist_ok=True)
    target.write_bytes(data if isinstance(data,bytes) else data.encode())


def main():
    global STAGE
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stage',type=Path,default=STAGE)
    STAGE=parser.parse_args().stage
    rules=build_rules()
    write(PLUGIN+'.esp',build_esp())
    write('SKSE/Plugins/InventoryInjector/'+PLUGIN+'.json',json.dumps(rules,indent=2)+'\n')
    write('Interface/'+ICON_SOURCE,build_swf())
    write('Interface/RenamePotionsI4/weapon_oil.svg',svg('#FFFFFF'))
    write('SKSE/Plugins/wheeler/I4.ini',
          '; Rename Potions + I4 compatibility. Merge these keys if you have a custom I4.ini.\n'
          '[I4]\nEnabled = true\nPreferI4Icons = true\nUseForPotions = true\nUseForPoisons = true\n'
          '; The included oil SVG does not require SWF extraction.\n')
    write('Documentation/RenamePotionsI4/weapon-oil.svg',svg())
    names=('Rename Potions + I4 v1.3: supported names\n\n'
           'ANY POISON NAME CONTAINING: weapon oil\n'
           'Case-insensitive. The phrase can appear anywhere in the name.\n'
           'Examples: Weapon Oil of Jarrin Crown; Greater WEAPON OIL III.\n'
           'Requires the included InventoryInjector.dll; gives the oil icon and Weapon Oil Type label.\n'
           'Potions without the Poison flag do not match. The item still functions as poison.\n\n'
           'Additional exact-name profiles (including older oil aliases and color variants):\n\n')
    for rule in rules['rules']:
        if 'contains' in rule['match']['text']: continue
        names += rule['$comment'].split(':')[0]+'\n'
        names += '\n'.join(rule['match']['text']['anyOf'])+'\n\n'
    write('Documentation/RenamePotionsI4/Supported-Names.txt',names)
    print(json.dumps({'rules':len(rules['rules']),
                      'supported_exact_names':sum(len(r['match']['text']['anyOf']) for r in rules['rules']),
                      'swf_bytes':len(build_swf()),'esp_bytes':len(build_esp())},indent=2))


if __name__=='__main__': main()
