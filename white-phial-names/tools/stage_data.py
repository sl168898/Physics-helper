from pathlib import Path
import argparse,base64,hashlib,json,shutil,subprocess,sys
p=argparse.ArgumentParser();p.add_argument('stage',type=Path);a=p.parse_args()
root=Path(__file__).resolve().parent.parent
manifest=json.loads((root/'data/PapyrusBuild.json').read_text())
(a.stage/'Scripts').mkdir(parents=True,exist_ok=True)
(a.stage/'Source/Scripts').mkdir(parents=True,exist_ok=True)
for name,hashes in manifest['scripts'].items():
 source=root/'papyrus'/f'{name}.psc'
 assert hashlib.sha256(source.read_bytes().replace(b'\r\n',b'\n')).hexdigest()==hashes['source_sha256_lf']
 payload=base64.b64decode((root/'data'/f'{name}.pex.b64').read_bytes())
 assert hashlib.sha256(payload).hexdigest()==hashes['pex_sha256']
 (a.stage/'Scripts'/f'{name}.pex').write_bytes(payload)
 shutil.copyfile(source,a.stage/'Source/Scripts'/source.name)
subprocess.run([sys.executable,str(root/'tools/build_plugin.py'),str(a.stage/'White Phial - Decanting.esp')],check=True)
(a.stage/'SEQ').mkdir(exist_ok=True)
(a.stage/'SEQ/White Phial - Decanting.seq').write_bytes(base64.b64decode((root/'data/quest_sequence.b64').read_bytes()))
shutil.copyfile(root/'data/PapyrusBuild.json',a.stage/'PapyrusBuild.json')
