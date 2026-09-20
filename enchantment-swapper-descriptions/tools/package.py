"""Assemble and verify a private MO2 overlay from both CI builds and local PEX files.

Usage: package.py TASK_DIRECTORY NATIVE_BUILD_ZIP WHEELER_BUILD_ZIP
The user-supplied original mod is intentionally not part of this repository.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('task_directory', type=Path)
parser.add_argument('native_build_zip', type=Path)
parser.add_argument('wheeler_build_zip', type=Path)
args = parser.parse_args()
root = args.task_directory.resolve()
source = Path(__file__).resolve().parents[1]
sha = lambda value: hashlib.sha256(value).hexdigest()
def read_archive(path):
    entries = {}
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
        for name in archive.namelist():
            if name.endswith('/'):
                continue
            normalized = name.replace('\\', '/')
            assert not normalized.startswith('/') and '..' not in Path(normalized).parts
            assert normalized.lower() not in {key.lower() for key in entries}
            entries[normalized] = archive.read(name)
    return entries

def verify_pe(dll, exports):
    assert dll[:2] == b'MZ'
    pe = struct.unpack_from('<I', dll, 0x3C)[0]
    assert dll[pe:pe+4] == b'PE\0\0'
    assert struct.unpack_from('<H', dll, pe+4)[0] == 0x8664
    assert struct.unpack_from('<H', dll, pe+24)[0] == 0x20B
    for name in exports:
        assert name.encode()+b'\0' in dll, name

contents = read_archive(args.native_build_zip)
dll_name = 'SKSE/Plugins/EnchantmentSwapperDescriptions.dll'
dll = contents[dll_name]
verify_pe(dll, ['SKSEPlugin_Load', 'SKSEPlugin_Version', 'ESD_GetDescriptionV1'])
info = json.loads(contents['BuildInfo.json'].decode('utf-8-sig'))
assert info['version'] == '1.1.0' and info['runtime'] == '1.6.1170'
assert sha(dll) == info['dll_sha256']
assert info['windows_build'] == 'passed' and info['rules_tests'] == 'passed'
for name, digest in info['source_sha256_lf'].items():
    assert sha((source/name).read_text().replace('\r\n', '\n').encode()) == digest, name

wheeler = read_archive(args.wheeler_build_zip)
wheeler_info = json.loads(wheeler.pop('BUILD-INFO.json').decode('utf-8-sig'))
wheeler_dll = wheeler['SKSE/Plugins/wheeler.dll']
verify_pe(wheeler_dll, ['SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version',
                       'GetWheelerAPI', 'GetInputBrokerAPI'])
assert sha(wheeler_dll) == wheeler_info['dllSHA256']
assert wheeler_info['patchVersion'] == '1.1.0'
assert wheeler_info['targetRefined'] == '1.3.3.0'
assert wheeler_info['refinedCommit'] == 'e3360bf81d05f739d2caa94d50e64e174b0ce1f8'
assert wheeler_info['patchCommit'] == info['source_commit']
patch = wheeler['Source/wheeler-compat.patch']
assert sha(patch) == wheeler_info['patchSHA256']
assert patch.replace(b'\r\n', b'\n') == (source/'wheeler/wheeler-compat.patch').read_bytes().replace(b'\r\n', b'\n')
for marker in [b'ESD_GetDescriptionV1', b'alchemyInventoryName',
               b'Rename Potions compatibility patch v1.0 enabled']:
    assert marker in wheeler_dll, marker
wheeler['Wheeler-BuildInfo.json'] = json.dumps(wheeler_info, indent=2).encode()+b'\n'
assert not ({key.lower() for key in contents} & {key.lower() for key in wheeler})
contents.update(wheeler)
for name in ['cdc_enchantmentswapperscript', 'ESD_Native']:
    pex = (root/'compiled'/f'{name}.pex').read_bytes()
    assert pex[:4] == bytes.fromhex('fa57c0de')
    contents[f'scripts/{name}.pex'] = pex
    contents[f'source/{name}.psc'] = (root/'patched-source'/f'{name}.psc').read_bytes()
for name in ['src/main.cpp', 'src/Bindings.h', 'src/DescriptionText.h', 'src/PublicAPI.h',
             'tests/bindings_tests.cpp', 'tests/description_tests.cpp',
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
info['wheeler_dll_sha256'] = sha(wheeler_dll)
info['wheeler_refined_version'] = '1.3.3.0'
contents['BuildInfo.json'] = json.dumps(info, indent=2).encode()+b'\n'
contents['Manifest.json'] = json.dumps({name: sha(value) for name, value in sorted(contents.items())}, indent=2).encode()+b'\n'
assert not any(name.lower().endswith(('.esp', '.esm', '.esl', '.swf')) for name in contents)
assert len([name for name in contents if name.lower().endswith('.dll')]) == 2
destination = root/'outputs/Enchantment_Swapper_Description_Fix_v1_1_0.zip'
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
