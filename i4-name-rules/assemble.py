"""Assemble v1.3 from the verified v1.2 mod and new I4 keyword component."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import zipfile

ROOT = Path(__file__).resolve().parent
BASE_SHA = '2bdd24373b6fd59e3d784fbbeb1e08b559e6567a4056e5b10d339ea367420881'
WHEELER_SHA = '7c4f2cf36c4d4bbbd482d5a1e2bbcc539c6b71ce864451e6a1205956fdf039c8'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def extract(archive, stage):
    with zipfile.ZipFile(archive) as z:
        for entry in z.infolist():
            target = (stage / entry.filename).resolve()
            if not target.is_relative_to(stage.resolve()):
                raise ValueError('Unsafe archive path')
        z.extractall(stage)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--base-v12', required=True, type=Path)
    parser.add_argument('--i4-component', required=True, type=Path)
    parser.add_argument('--stage', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--packaging-commit', default='local')
    args = parser.parse_args()
    if sha(args.base_v12) != BASE_SHA: raise ValueError('Unexpected base v1.2 ZIP')
    stage = args.stage
    if stage.exists() and any(stage.iterdir()): raise ValueError('Use an empty staging directory')
    stage.mkdir(parents=True, exist_ok=True)
    extract(args.base_v12, stage)
    (stage/'BUILD-INFO.json').rename(stage/'WHEELER-BUILD-INFO.json')
    old_tools = stage/'Source/Wheeler-Patch-v1.2'
    old_tools.mkdir()
    for file in (stage/'Source').iterdir():
        if file.name not in ['Wheeler-Refined','Wheeler-Patch-v1.2']:
            shutil.move(str(file), str(old_tools/file.name))
    extract(args.i4_component, stage)
    i4_info = json.loads((stage/'I4-KEYWORD-BUILD-INFO.json').read_text(encoding='utf-8-sig'))
    if i4_info['patchVersion'] != '1.3': raise ValueError('Wrong I4 component')
    if sha(stage/'SKSE/Plugins/InventoryInjector.dll') != i4_info['dllSHA256']:
        raise ValueError('I4 DLL hash mismatch')
    if sha(stage/'SKSE/Plugins/wheeler.dll') != WHEELER_SHA:
        raise ValueError('Wheeler v1.2 preservation check failed')
    subprocess.run([sys.executable,str(ROOT/'build_patch.py'),'--stage',str(stage)],check=True)
    subprocess.run([sys.executable,str(ROOT/'validate_assets.py'),'--stage',str(stage)],check=True)
    shutil.copyfile(ROOT/'asset-validation.json',stage/'VALIDATION.json')
    shutil.copyfile(ROOT/'README-COMBINED.md',stage/'README-Rename-Potions-Patch.md')
    source = stage/'Source/Combined-Patch-v1.3'
    source.mkdir()
    for name in ['assemble.py','build_patch.py','validate_assets.py','README-COMBINED.md']:
        shutil.copyfile(ROOT/name,source/name)
    info = {
        'patchVersion':'1.3', 'targetRuntime':'Steam Skyrim SE 1.6.1170',
        'packagingCommit':args.packaging_commit,
        'nameRule':{'contains':'weapon oil','ignoreCase':True,'requiresPoison':True},
        'typeDisplay':'Weapon Oil', 'gameplayChanged':False,
        'inventoryInjector':i4_info,
        'wheeler':{'compatibilityVersion':'1.2','refinedVersion':'1.3.3.0',
                   'dllSHA256':WHEELER_SHA,'unchangedFromV12':True,
                   'preserved':'batch-name memory, I4 name forwarding, Enchantment Swapper descriptions'},
        'baseArchiveSHA256':BASE_SHA,'i4ComponentArchiveSHA256':sha(args.i4_component),
        'inGameTested':False
    }
    (stage/'BUILD-INFO.json').write_text(json.dumps(info,indent=2)+'\n')
    files = sorted(p for p in stage.rglob('*') if p.is_file())
    sums = ''.join(f'{sha(p)}  {p.relative_to(stage).as_posix()}\n' for p in files)
    (stage/'SHA256SUMS.txt').write_text(sums)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(args.output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(stage.rglob('*')):
            if p.is_file(): z.write(p,p.relative_to(stage).as_posix())
    print(json.dumps({'archive':str(args.output),'sha256':sha(args.output),'bytes':args.output.stat().st_size}))

if __name__ == '__main__': main()
