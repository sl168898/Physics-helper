"""Verify the exact Windows Twin Shot build against source and delivered 0.4.0."""
from pathlib import Path
import hashlib, io, json, struct, subprocess, sys, zipfile
from make_coating_perks import build, validate

def sha(data): return hashlib.sha256(data).hexdigest()
def read_zip(path):
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None
        names=[n for n in z.namelist() if not n.endswith('/')]
        assert len(names)==len(set(names))
        return {n:z.read(n) for n in names}

def verify_dll_version(blob, packed_version=0x00050000):
    u16 = lambda pos: struct.unpack_from('<H', blob, pos)[0]
    u32 = lambda pos: struct.unpack_from('<I', blob, pos)[0]
    assert blob[:2] == b'MZ'
    pe = u32(0x3c)
    assert blob[pe:pe+4] == b'PE\0\0' and u16(pe+4) == 0x8664
    optional = pe+24
    assert u16(optional) == 0x20b
    sections = optional+u16(pe+20)

    def raw(rva):
        if rva < u32(optional+60):
            return rva
        for i in range(u16(pe+6)):
            section = sections+40*i
            va, size = u32(section+12), u32(section+16)
            if va <= rva < va+size:
                return u32(section+20)+rva-va
        raise ValueError(f'Unmapped RVA {rva:x}')

    def string(pos):
        return blob[pos:blob.index(b'\0', pos)].decode()

    export = raw(u32(optional+112))
    functions, names, ordinals = (raw(u32(export+offset)) for offset in (28, 32, 36))
    for i in range(u32(export+24)):
        if string(raw(u32(names+4*i))) == 'SKSEPlugin_Version':
            version = raw(u32(functions+4*u16(ordinals+2*i)))
            assert u32(version) == 1 and u32(version+4) == packed_version
            assert string(version+8) == 'PoisonedAmmoNative'
            return
    raise AssertionError('No SKSEPlugin_Version export')


def perk_records(blob):
    out={}
    def walk(start,end):
        while start<end:
            sig,size=struct.unpack_from('<4sI',blob,start)
            if sig==b'GRUP': walk(start+24,start+size);start+=size
            else:
                if sig==b'PERK': out[struct.unpack_from('<I',blob,start+12)[0]]=blob[start:start+24+size]
                start+=24+size
        assert start==end
    walk(0,len(blob));return out

def verify(baseline,artifact,project,output):
    assert sha(baseline.read_bytes())=='d374a1d9316953394426a272549f8d86bd2933834da633762346daad8bed53df'
    outer=read_zip(artifact)
    if 'BuildInfo.json' in outer: payload=artifact.read_bytes()
    else:
        archives=[(n,b) for n,b in outer.items() if n.endswith('.zip')]
        assert len(archives)==1 and archives[0][0]=='Poisoned_Ammunition_Coating_Perks_v0_5_0_Beta.zip'
        payload=archives[0][1]
    new=read_zip(io.BytesIO(payload));old=read_zip(baseline)
    info=json.loads(new['BuildInfo.json'].decode('utf-8-sig'))
    assert info['version']=='0.5.0-beta' and info['runtime']=='Steam 1.6.1170'
    for key in ('windows_build','portable_tests','esp_structure_validation','production_impact_wrapper_test',
                'icon_metadata_tests','reload_sequence_tests','production_poison_snapshot_tests',
                'potency_production_hook_tests','twin_shot_production_tests'):
        assert info[key]=='passed',key
    assert info['native_test_suites']==7 and info['in_game_tested'] is False
    assert info['twin_shot_marksman']==80 and info['twin_shot_parent']=='Alchemical Potency'
    assert info['twin_shot_projectiles']==info['twin_shot_ammo_cost']==2
    assert info['twin_shot_extra_spread_degrees']==0.75
    for key in ('twin_shot','twin_shot_last_bolt_single','twin_shot_same_coating_batch','twin_shot_deferred_extra',
                'twin_shot_extra_launch_failure_refund','twin_shot_no_recursive_duplication','twin_shot_stale_load_cancelled'):
        assert info[key] is True,key
    assert info['twin_shot_native_launch_relocation']==[42928,44108]
    assert subprocess.check_output(['git','-C',str(project),'rev-parse','HEAD'],text=True).strip()==info['source_commit']
    for name,digest in info['source_sha256_lf'].items():
        assert sha((project/name).read_text().encode())==digest,name
    dll=new['SKSE/Plugins/PoisonedAmmoNative.dll'];verify_dll_version(dll)
    assert sha(dll)==info['dll_sha256']
    assert sha(new['CoatingMechanist.esp'])==info['perks_sha256']
    assert sha(new['PoisonedAmmoNative.esp'])==info['esp_sha256']
    assert new['PoisonedAmmoNative.esp']==old['PoisonedAmmoNative.esp']
    validate(new['CoatingMechanist.esp']);assert new['CoatingMechanist.esp']==build()
    previous=perk_records(old['CoatingMechanist.esp']);current=perk_records(new['CoatingMechanist.esp'])
    assert len(previous)==4 and len(current)==5
    assert all(current[k]==v for k,v in previous.items())
    key='SKSE/Plugins/PerkAdjuster/CoatingMechanist.json'
    previous_tree=json.loads(old[key]);tree=json.loads(new[key]);assert tree['additions'][:4]==previous_tree['additions']
    assert tree['additions'][4]['parents']==['0x803|CoatingMechanist.esp']
    allowed={'SKSE/Plugins/PoisonedAmmoNative.dll','CoatingMechanist.esp',key,'README.md','TESTING.md','BuildInfo.json','SOURCE.txt'}
    assert set(new)==set(old)
    changed={n for n in old if old[n]!=new[n]};assert changed<=allowed,changed-allowed
    assert b'PoisonedAmmoNative_GetIconInfoV1' in dll and b'PoisonedAmmoNative_CoatOneV1' in dll
    output.parent.mkdir(parents=True,exist_ok=True);output.write_bytes(payload)
    print(json.dumps({'version':'0.5.0-beta','source_commit':info['source_commit'],'windows_build':'passed',
        'native_test_suites':7,'unchanged_existing_perks':4,'new_perk':'Twin Shot',
        'unchanged_recipe_esp':True,'changed_files':sorted(changed),'dll_sha256':sha(dll),
        'perks_sha256':info['perks_sha256'],'archive_sha256':sha(payload),'archive_bytes':len(payload),
        'in_game_tested':False},indent=2))

if __name__=='__main__': verify(*map(Path,sys.argv[1:5]))
