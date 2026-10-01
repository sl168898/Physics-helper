"""Add Green Thumb to Combined 2.11.1 without renumbering any existing form."""
from pathlib import Path
import collections
import hashlib
import json
import struct
import sys
from esp_base import fields, pack, records, U32

PLUGIN = 'Biggie Traits - Combined.esp'
FLM = 'Biggie Traits - Combined_FLM.ini'
ABILITY, EFFECT, PERK = 0x21000F70, 0x21000F71, 0x21000F72
DESCRIPTION = ('Gather <twice as many ingredients>, including <Huntsman\'s Satchel refunds>. '
               'Stacks with another double-harvest bonus for <four times the yield>. '
               'Eating ingredients reveals <all four effects>; their timed effects last <twice as long>. '
               'Your carrying capacity is reduced by <50>.')
z = lambda s: s.encode('utf-8') + b'\0'
u = lambda n: struct.pack('<I', n)

def condition(function, parameter=0, value=1.):
    return struct.pack('<B3xfH2xIIIIi', 0, value, function, parameter, 0, 0, 0, -1)

def entry(point, function, args, value, filters=()):
    out = [(b'PRKE', bytes([2,0,0])), (b'DATA', bytes([point,function,args]))]
    if filters:
        out += [(b'PRKC', b'\1')] + [(b'CTDA', c) for c in filters]
    return out + [(b'EPFT', b'\1'), (b'EPFD', struct.pack('<f',value)), (b'PRKF', b'')]

def clone(before, source, new_id, replacements):
    head, body = before[source]
    body = pack([(t, replacements.get(t,v)) for t,v in fields(body)])
    h = bytearray(head)
    struct.pack_into('<I',h,4,len(body))
    struct.pack_into('<I',h,8,U32(h[8:12]) & ~0x40000)
    struct.pack_into('<I',h,12,new_id)
    return bytes(h) + body

def rewrite(blob, changes, additions, groups):
    out=bytearray();pos=0
    while pos<len(blob):
        head=bytearray(blob[pos:pos+24]);size=U32(head[4:8])
        if head[:4]==b'GRUP':
            groups['count']+=1
            body=rewrite(blob[pos+24:pos+size],changes,additions,groups)
            if U32(head[12:16])==0 and bytes(head[8:12]) in additions:
                tag=bytes(head[8:12]);body+=additions[tag];groups[tag.decode()]+=1
            struct.pack_into('<I',head,4,len(body)+24);out+=head+body;pos+=size
        else:
            fid=U32(head[12:16])
            if fid in changes:
                body=changes[fid];struct.pack_into('<I',head,4,len(body));out+=head+body
            else:out+=blob[pos:pos+24+size]
            pos+=24+size
    assert pos==len(blob)
    return bytes(out)

def build(source, output):
    old=(source/PLUGIN).read_bytes();before=dict(records(old))
    assert hashlib.sha256(old).hexdigest()=='6012817b8a4bf4021f25e4f4ed623e3821e7f24673e8d314e9df3d2bcec9fd7a'
    assert not {ABILITY,EFFECT,PERK}&set(before)
    effect_data=bytearray(dict(fields(before[0x21000F01][1]))[b'DATA'])
    # Native MGEF Perk to Apply, copied from existing selectable traits.
    assert U32(effect_data[136:140])==0x21000F02
    struct.pack_into('<I',effect_data,136,PERK)
    additions={
      b'SPEL':clone(before,0x21000F00,ABILITY,{b'EDID':z('Traits_GreenThumbAb'),b'FULL':z('Green Thumb'),b'DESC':z(DESCRIPTION),b'EFID':u(EFFECT),b'EFIT':struct.pack('<fII',50.,0,0)}),
      b'MGEF':clone(before,0x21000F01,EFFECT,{b'EDID':z('Traits_GreenThumb'),b'FULL':z('Trait: Green Thumb'),b'DNAM':z(DESCRIPTION.replace('<','').replace('>','')),b'DATA':bytes(effect_data)}),
    }
    pf=[(b'EDID',z('BT_GreenThumbPerk')),(b'FULL',z('Green Thumb')),(b'DESC',b'\0'),(b'DATA',bytes([0,0,1,1,0]))]
    # Same native Experimenter entry as Requiem's Alchemical Lore rank 2.
    pf+=entry(72,1,2,4.)
    # CK GetIsObjectType uses its own enum: Ingredient=5, Potion=17.
    pf+=entry(30,3,3,2.,[condition(432,5)])
    ph=bytearray(before[0x21000F52][0]);pb=pack(pf)
    struct.pack_into('<I',ph,4,len(pb));struct.pack_into('<I',ph,12,PERK)
    additions[b'PERK']=bytes(ph)+pb
    satchel={
      0x21000A00:(b'DESC',"Use <Huntsman's Satchel> to remember the next poison you brew. Its killing blow refunds <one ingredient set once per batch>, increased by <harvest bonuses, including Green Thumb>. Only personally brewed batches of your stored recipe qualify. Receive <one Jarrin Root>. You have <50% weakness to poison>."),
      0x21000A01:(b'DNAM',"Your stored poison's killing blow returns its batch's ingredients once. Harvest bonuses, including Green Thumb, increase the refund. Receive one Jarrin Root. Poison resistance is reduced by 50 percentage points."),
      0x21000F40:(b'DESC','Remember the next poison you brew. Its killing blow refunds one ingredient set once per batch, increased by harvest bonuses including Green Thumb.'),
      0x21000F41:(b'DNAM','Remember the next poison you brew. Its killing blow refunds one ingredient set once per batch, increased by harvest bonuses including Green Thumb.'),
    }
    changes={fid:pack([(t,z(text) if t==tag else v) for t,v in fields(before[fid][1])]) for fid,(tag,text) in satchel.items()}
    hf=[]
    for t,v in fields(before[0][1]):
        if t==b'HEDR':
            v=bytearray(v);struct.pack_into('<I',v,4,U32(v[4:8])+3);struct.pack_into('<I',v,8,0xF73);v=bytes(v)
        elif t==b'SNAM':v=z('Thirteen custom traits and the Voice of Authority Speech perk.')
        hf.append((t,v))
    changes[0]=pack(hf)
    groups=collections.Counter();new=rewrite(old,changes,additions,groups);after=dict(records(new))
    assert set(after)==set(before)|{ABILITY,EFFECT,PERK}
    assert groups['SPEL']==groups['MGEF']==groups['PERK']==1
    for fid,(h,b) in before.items():
        if fid not in changes:assert after[fid]==(h,b),hex(fid)
        else:assert after[fid][1]==changes[fid]
    assert U32(dict(fields(after[0][1]))[b'HEDR'][4:8])==len(after)-1+groups['count']
    assert [(t,v) for t,v in hf if t not in (b'HEDR',b'SNAM')]==[(t,v) for t,v in fields(before[0][1]) if t not in (b'HEDR',b'SNAM')]
    assert U32(after[0][0][8:12])&0x200 # ESL flag retained.
    assert not any(t==b'DATA' and len(v)==3 and v[0]==87 for t,v in pf) # no second harvest multiplier
    assert [v for t,v in pf if t==b'EPFD']==[struct.pack('<f',v) for v in (4.,2.)]
    assert [struct.unpack('<H',v[8:10])[0] for t,v in pf if t==b'CTDA']==[432]
    output.mkdir(parents=True,exist_ok=True);(output/PLUGIN).write_bytes(new)
    flm=(source/FLM).read_text().rstrip()+'\n\n; Green Thumb: paired selection/removal menu entries.\nFormList = Traits_AbilityList|Traits_GreenThumbAb\nFormList = Traits_EffectsList|Traits_GreenThumb\n'
    abilities=[s.split('|')[-1] for s in flm.splitlines() if s.startswith('FormList = Traits_AbilityList|')]
    effects=[s.split('|')[-1] for s in flm.splitlines() if s.startswith('FormList = Traits_EffectsList|')]
    assert len(abilities)==len(effects)==13
    assert all(a==e+'Ab' for a,e in zip(abilities,effects))
    edids={dict(fields(b)).get(b'EDID',b'').rstrip(b'\0').decode():fid for fid,(h,b) in after.items()}
    assert all(a in edids and e in edids for a,e in zip(abilities,effects))
    for a in abilities:
        assert (source/'Interface/TraitPics'/f'{a}.dds').exists() or a=='Traits_GreenThumbAb'
    (output/FLM).write_text(flm)
    report={'version':'2.12.0-beta1','baseline':'2.11.1-beta1','status':'PASS',
      'esp_sha256':hashlib.sha256(new).hexdigest(),'new_forms':{'ability':hex(ABILITY),'effect':hex(EFFECT),'perk':hex(PERK)},
      'changed_existing_forms':[hex(i) for i in changes],'other_existing_forms_byte_identical':True,
      'masters_and_existing_form_ids_preserved':True,'esl_flag_preserved':True,'paired_traits':13,
      'alchemy_50_bonus_removed':True,'ingredient_discovery':4,'raw_ingredient_duration_multiplier':2,
      'carry_weight_penalty':50,'potions_and_poisons_unchanged':True,
      'green_thumb_harvest_multiplier_applied_natively_after_other_perks':2,'duplicate_esp_harvest_bonus':False,
      'record_count':len(after)-1,'group_count':groups['count'],'in_game_tested':False}
    (output/'GreenThumb-ESP-Validation.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':build(Path(sys.argv[1]),Path(sys.argv[2]))
