"""Assemble and verify a private MO2 overlay from a CI DLL and local PEX files.

Usage: package.py TASK_DIRECTORY NATIVE_BUILD_ZIP
The user-supplied original mod is intentionally not part of this repository.
"""
from pathlib import Path
import argparse
import hashlib
import io
import json
import struct
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('task_directory', type=Path)
parser.add_argument('native_build_zip', type=Path)
args = parser.parse_args()
root = args.task_directory.resolve()
source = Path(__file__).resolve().parents[1]
sha = lambda value: hashlib.sha256(value).hexdigest()
contents = {}
with zipfile.ZipFile(args.native_build_zip) as archive:
    assert archive.testzip() is None
    for name in archive.namelist():
        if name.endswith('/'):
            continue
        normalized = name.replace('\\', '/')
        assert not normalized.startswith('/') and '..' not in Path(normalized).parts
        contents[normalized] = archive.read(name)
dll_name = 'SKSE/Plugins/EnchantmentSwapperDescriptions.dll'
dll = contents[dll_name]
assert dll[:2] == b'MZ'
pe = struct.unpack_from('<I', dll, 0x3C)[0]
assert dll[pe:pe+4] == b'PE\0\0'
assert struct.unpack_from('<H', dll, pe+4)[0] == 0x8664
assert struct.unpack_from('<H', dll, pe+24)[0] == 0x20B
assert b'SKSEPlugin_Load\0' in dll and b'SKSEPlugin_Version\0' in dll
info = json.loads(contents['BuildInfo.json'].decode('utf-8-sig'))
assert sha(dll) == info['dll_sha256']
assert info['windows_build'] == 'passed' and info['rules_tests'] == 'passed'
for name, digest in info['source_sha256_lf'].items():
    assert sha((source/name).read_text().replace('\r\n', '\n').encode()) == digest, name
for name in ['cdc_enchantmentswapperscript', 'ESD_Native']:
    pex = (root/'compiled'/f'{name}.pex').read_bytes()
    assert pex[:4] == bytes.fromhex('fa57c0de')
    contents[f'scripts/{name}.pex'] = pex
    contents[f'source/{name}.psc'] = (root/'patched-source'/f'{name}.psc').read_bytes()
for name in ['src/main.cpp', 'src/Bindings.h', 'tests/bindings_tests.cpp',
             'CMakeLists.txt', 'vcpkg.json', 'tools/build_windows.ps1',
             'tools/patch_script.py', 'tools/package.py', 'LICENSE']:
    contents['Source/EnchantmentSwapperDescriptions/'+name] = (source/name).read_bytes()
contents['README.md'] = (source/'README.md').read_bytes()
contents['MinHook-LICENSE.txt'] = (source.parent/'three-traits/MinHook-LICENSE.txt').read_bytes()
info['validation'] = json.loads((root/'validation.json').read_text())
info['original_mod_version'] = 'Enchantment Swapper 2.0 (uploaded archive)'
info['original_script_sha256'] = sha((root/'inputs/base/scripts/cdc_enchantmentswapperscript.pex').read_bytes())
info['patched_script_sha256'] = sha(contents['scripts/cdc_enchantmentswapperscript.pex'])
info['native_declaration_sha256'] = sha(contents['scripts/ESD_Native.pex'])
info['replaces_esp'] = False
contents['BuildInfo.json'] = json.dumps(info, indent=2).encode()+b'\n'
contents['Manifest.json'] = json.dumps({name: sha(value) for name, value in sorted(contents.items())}, indent=2).encode()+b'\n'
assert not any(name.lower().endswith(('.esp', '.esm', '.esl', '.swf')) for name in contents)
assert len([name for name in contents if name.lower().endswith('.dll')]) == 1
destination = root/'outputs/Enchantment_Swapper_Description_Fix_v1_0_0.zip'
destination.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(destination, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name, data in sorted(contents.items()):
        entry = zipfile.ZipInfo(name, (2026, 9, 20, 0, 0, 0))
        entry.compress_type = zipfile.ZIP_DEFLATED
        entry.external_attr = 0o644 << 16
        archive.writestr(entry, data)
with zipfile.ZipFile(destination) as archive:
    assert archive.testzip() is None
    manifest = json.loads(archive.read('Manifest.json'))
    assert all(sha(archive.read(name)) == digest for name, digest in manifest.items())
print(json.dumps({'file': str(destination), 'size': destination.stat().st_size,
                  'sha256': sha(destination.read_bytes()), 'files': len(contents)}, indent=2))
