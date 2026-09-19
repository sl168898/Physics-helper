from pathlib import Path, PurePosixPath
import hashlib
import json
import sys
import zipfile

sys.path.insert(0, '/tmp/soul_recharge_deps')
import pefile

root = Path(__file__).resolve().parent
archive = root / 'Wheeler_Refined_Rename_Potions_Patch_v1_0.zip'
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None, 'ZIP checksum failure'
    names = z.namelist()
    assert len(names) == len(set(names)), 'Duplicate ZIP entries'
    assert all(not PurePosixPath(n).is_absolute() and '..' not in PurePosixPath(n).parts for n in names)
    dlls = [n for n in names if n.lower().endswith('.dll')]
    assert dlls == ['SKSE/Plugins/wheeler.dll'], dlls
    assert not any(n.lower().endswith(('.esp', '.esl', '.esm', '.pex', '.ini')) and not n.startswith('Source/') for n in names)
    data = z.read(dlls[0])
    info = json.loads(z.read('BUILD-INFO.json').decode('utf-8-sig'))
    assert hashlib.sha256(data).hexdigest() == info['dllSHA256']
    patch = (root/'rename-potions.patch').read_bytes()
    packaged_patch = z.read('Source/rename-potions.patch')
    assert hashlib.sha256(packaged_patch).hexdigest() == info['patchSHA256']
    assert packaged_patch.replace(b'\r\n', b'\n') == patch.replace(b'\r\n', b'\n')
    assert info['inGameTested'] is False
    assert info['refinedCommit'] == 'e3360bf81d05f739d2caa94d50e64e174b0ce1f8'
    assert z.read('README-Rename-Potions-Patch.md').decode('utf-8-sig').replace('\r\n', '\n') == (root/'README.md').read_text()
    for name in ['AlchemyBatch.h', 'WheelItemAlchemy.cpp', 'WheelItemAlchemy.h', 'WheelItemFactory.cpp', 'WheelItemMissing.cpp', 'WheelItemMissing.h']:
        path = 'src/bin/Wheeler/WheelItems/' + name
        assert z.read('Source/Wheeler-Refined/' + path).replace(b'\r\n', b'\n') == (root/'repo'/path).read_bytes().replace(b'\r\n', b'\n'), path
    for path in ['src/bin/Wheeler/Wheeler.cpp', 'src/bin/Wheeler/Wheeler.h', 'src/bin/Utilities/InventorySnapshotCache.cpp', 'src/bin/main.cpp']:
        assert z.read('Source/Wheeler-Refined/' + path).replace(b'\r\n', b'\n') == (root/'repo'/path).read_bytes().replace(b'\r\n', b'\n'), path
    for name in ['Licenses/LICENSE', 'Licenses/LICENSES/BSD-3-Clause-Wheeler.txt', 'Licenses/CommonLibSSE-LICENSE', 'Source/tools/build_windows.ps1', 'Source/tests/batch_tests.cpp']:
        assert len(z.read(name)) > 50, name

pe = pefile.PE(data=data)
assert pe.FILE_HEADER.Machine == 0x8664
assert pe.OPTIONAL_HEADER.Magic == 0x20b
exports = {e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
assert {'SKSEPlugin_Load', 'SKSEPlugin_Query', 'SKSEPlugin_Version', 'GetWheelerAPI', 'GetInputBrokerAPI'} <= exports, exports
imports = sorted(i.dll.decode() for i in pe.DIRECTORY_ENTRY_IMPORT)
assert not any(n.lower() in ['ucrtbased.dll', 'vcruntime140d.dll', 'msvcp140d.dll'] for n in imports)
assert b'alchemyInventoryName' in data
assert b'Rename Potions compatibility patch v1.0 enabled' in data
result = {'archive':archive.name, 'archiveSHA256':hashlib.sha256(archive.read_bytes()).hexdigest(),
          'bytes':archive.stat().st_size, 'dllSHA256':info['dllSHA256'], 'exports':sorted(exports),
          'imports':imports, 'patchCommit':info['patchCommit'], 'packageValidation':'passed', 'inGameTested':False}
(root/'validation-result.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
