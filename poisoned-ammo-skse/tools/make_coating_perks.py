"""Create three additive perk records; no AVIF or existing-record overrides."""
from pathlib import Path
import json, struct, sys
from make_plugin import sub, text, record, group
OWN=0x01000000

def condition(function, parameter, comparison, operator=0):
    # CTDA: comparison operator in bits 5..7; subject; no external reference.
    return sub('CTDA', struct.pack('<B3sfH2sIIIIi', operator<<5, bytes(3), comparison,
                                  function, bytes(2), parameter, 0, 0, 0, -1))

def perk(local, name, level, description, parent=None):
    body=text('EDID', ['CM_CoatingMechanistI','CM_CoatingMechanistII','CM_MeasuredDose'][local-0x800])
    body+=text('FULL',name)+text('DESC',description)
    body+=condition(277,8,float(level),3)  # GetBaseActorValue Marksman >= level
    if parent is not None: body+=condition(448,OWN+parent,1.0)  # HasPerk
    body+=sub('DATA',bytes([0,1,1,1,0]))  # Not trait; level 1; one rank; playable; visible
    return record('PERK',OWN+local,body)

def build():
    perks=[
      perk(0x800,'Coating Mechanist I',25,'Poisons and weapon oils delivered by your crossbow bolts are 25% stronger.'),
      perk(0x801,'Coating Mechanist II',50,'Poisons and weapon oils delivered by your crossbow bolts are 50% stronger. Replaces the bonus from Coating Mechanist I.',0x800),
      perk(0x802,'Measured Dose',30,'Each poison or weapon-oil bottle coats twice as many crossbow bolts. Combines with Coating Mechanist and existing poison-dose bonuses.')]
    header=record('TES4',0,sub('HEDR',struct.pack('<fII',1.7,4,0x803))+
      text('CNAM','Physics-helper contributors')+text('SNAM','Coating Mechanist perks. Requires PoisonedAmmoNative 0.2.0+ and Perk Adjuster.')+
      text('MAST','Skyrim.esm')+sub('DATA',bytes(8)),0x200)
    return header+group('PERK',perks)

def validate(blob):
    # Independent record/subrecord reader: reject overrides, AVIFs, or missing gates.
    out=[]
    def walk(a,end):
      while a<end:
        sig,size=struct.unpack_from('<4sI',blob,a)
        if sig==b'GRUP':
          assert blob[a+8:a+12]==b'PERK';walk(a+24,a+size);a+=size;continue
        assert a+24+size<=end
        flags,fid=struct.unpack_from('<II',blob,a+8);pos=a+24;subs=[]
        while pos<a+24+size:
          key,n=struct.unpack_from('<4sH',blob,pos);pos+=6
          subs.append((key,blob[pos:pos+n]));pos+=n
        assert pos==a+24+size
        out.append((sig,flags,fid,subs));a+=24+size
      assert a==end
    walk(0,len(blob));assert len(out)==4
    assert out[0][:3]==(b'TES4',0x200,0)
    assert [v for k,v in out[0][3] if k==b'MAST']==[b'Skyrim.esm\0']
    for index,(sig,flags,fid,subs) in enumerate(out[1:]):
      assert (sig,flags,fid)==(b'PERK',0,OWN+0x800+index)
      assert dict(subs)[b'DATA']==bytes([0,1,1,1,0])
      conditions=[struct.unpack('<B3sfH2sIIIIi',v) for k,v in subs if k==b'CTDA']
      assert len(conditions)==(2 if index==1 else 1)
      first=conditions[0];assert (first[0],first[2],first[3],first[5])==(0x60,[25,50,30][index],277,8)
      if index==1:
        second=conditions[1];assert (second[0],second[2],second[3],second[5])==(0,1,448,OWN+0x800)
      assert all(c[6:]==(0,0,0,-1) for c in conditions)
    return True

if __name__=='__main__':
    dest=Path(sys.argv[1]);blob=build();validate(blob);dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(blob)
    print('PASS: three new ESL perks; Marksman 25/50/30; rank II requires rank I; no skill-tree overrides')
