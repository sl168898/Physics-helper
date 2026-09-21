"""Add stable liquid slots without changing any original plugin ID or master."""
from pathlib import Path
import argparse, base64, collections, struct
from esp import records, fields, field, record, group
p = argparse.ArgumentParser(); p.add_argument('output', type=Path); a = p.parse_args()
root = Path(__file__).resolve().parent.parent
base = base64.b64decode((root/'data/base_plugin.b64').read_bytes(), validate=False)
original = list(records(base)); masters = [d for t,d in fields(original[0][1]) if t == b'MAST']
index = len(masters); assert index == 6
ids = [struct.unpack_from('<I', h, 12)[0] for h,b in original]
assert set(fid & 0xFFFFFF for fid in ids if fid >> 24 == index) == {0x800,0x801,0x802,0x803,*range(0x810,0x816)}
new = []
for local, name in ((0x804, 'WPD_StorageHashLow'), (0x805, 'WPD_StorageHashHigh')):
 body = field(b'EDID',name.encode()+b'\0')+field(b'FNAM',b'f')+field(b'FLTV',struct.pack('<f',0))
 new.append(record(b'GLOB', (index << 24)|local, body))
template = next(body for h,body in original if struct.unpack_from('<I',h,12)[0] == (index << 24)|0x810)
for local in range(0x900,0x1000):
 parts=[]
 for tag,data in fields(template):
  if tag == b'EDID': data=f'WPD_ProtectedLiquid_{local:03X}'.encode()+b'\0'
  elif tag == b'FULL': data=b'Protected liquid (definition unavailable)\0'
  elif tag in (b'EFID',b'EFIT',b'CTDA',b'CIS1',b'CIS2'): continue
  parts.append(field(tag,data))
 new.append(record(b'ALCH',(index << 24)|local,b''.join(parts)))
# Rebuild top-level groups, preserving every pre-existing record header/body.
# HEDR record count and next-object-ID are the only original bytes changed.
header,body=original[0]; updated=[]
for tag,data in fields(body):
 if tag==b'HEDR':
  version,count,nxt=struct.unpack('<fII',data)
  data=struct.pack('<fII',version,count+len(new),0x1000)
 updated.append(field(tag,data))
out=record(b'TES4',0,b''.join(updated),head=header)
groups=collections.OrderedDict()
for h,b in original[1:]: groups.setdefault(h[:4],[]).append(h+b)
for raw in new: groups.setdefault(raw[:4],[]).append(raw)
for tag,items in groups.items(): out+=group(tag,b''.join(items))
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_bytes(out)
parsed=list(records(out)); assert len(parsed)==len(original)+1794
allids=[struct.unpack_from('<I',h,12)[0] for h,b in parsed];assert len(set(allids))==len(allids)
assert [d for t,d in fields(parsed[0][1]) if t==b'MAST']==masters
oldmap={struct.unpack_from('<I',h,12)[0]:(h,b) for h,b in original[1:]}
newmap={struct.unpack_from('<I',h,12)[0]:(h,b) for h,b in parsed[1:]}
assert all(newmap[fid]==item for fid,item in oldmap.items())
assert struct.unpack_from('<I',parsed[0][0],8)[0] & 0x200
print(f'Preserved {len(original)-1} original records and all masters; added 1792 ALCH slots and two saved globals; ESL intact')
