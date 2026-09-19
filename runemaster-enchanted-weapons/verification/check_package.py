"""Check the downloaded Windows build against its reviewed source and plugin."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
package = Path(sys.argv[1])
expected_commit = sys.argv[2]
digest = lambda data: hashlib.sha256(data).hexdigest()
with zipfile.ZipFile(package) as archive:
    assert archive.testzip() is None
    names = {n for n in archive.namelist() if not n.endswith('/')}
    assert names == {
        'SKSE/Plugins/RunemasterEnchantmentBridge.dll',
        'Runemaster Enchanted Weapons.esp', 'BuildInfo.json',
        'README.md', 'LICENSE', 'CommonLibSSE-LICENSE',
    }, names
    info = json.loads(archive.read('BuildInfo.json').decode('utf-8-sig'))
    assert info['version'] == '1.2.0' and info['runtime'] == '1.6.1170'
    assert info['source_commit'] == expected_commit
    for field in ['windows_build', 'rules_tests', 'crafting_preservation_tests', 'inventory_hook_abi_tests', 'visual_lifecycle_tests']:
        assert info[field] == 'passed'
    assert info['independent_rune_visuals'] is True
    assert info['in_game_tested'] is False and info['permanent_weapons'] == 8
    for name, expected in info['source_sha256_lf'].items():
        assert digest((root / name).read_text().replace('\r\n', '\n').encode()) == expected, name
    plugin = archive.read('Runemaster Enchanted Weapons.esp')
    assert plugin == (root / 'stage/Runemaster Enchanted Weapons.esp').read_bytes()
    assert archive.read('README.md').decode().replace('\r\n', '\n') == (root / 'README.md').read_text()
    dll = archive.read('SKSE/Plugins/RunemasterEnchantmentBridge.dll')
    assert digest(dll) == info['dll_sha256']
    assert dll[:2] == b'MZ'
    pe = struct.unpack_from('<I', dll, 0x3c)[0]
    assert dll[pe:pe+4] == b'PE\0\0'
    machine, sections = struct.unpack_from('<HH', dll, pe+4)
    optional_size, characteristics = struct.unpack_from('<HH', dll, pe+20)
    assert machine == 0x8664 and characteristics & 0x2000  # AMD64 DLL
    optional = pe + 24
    assert struct.unpack_from('<H', dll, optional)[0] == 0x20b  # PE32+
    section_table = optional + optional_size
    mapping = []
    for i in range(sections):
        virtual_size, rva, size, offset = struct.unpack_from('<IIII', dll, section_table+i*40+8)
        mapping.append((rva, rva+max(virtual_size, size), offset))
    def locate(rva):
        for start, end, offset in mapping:
            if start <= rva < end:
                return offset + rva - start
        raise AssertionError(f'Unmapped RVA {rva:x}')
    export_rva, export_size = struct.unpack_from('<II', dll, optional+112)
    assert export_rva and export_size
    exports = locate(export_rva)
    name_count = struct.unpack_from('<I', dll, exports+24)[0]
    name_array = locate(struct.unpack_from('<I', dll, exports+32)[0])
    export_names = []
    for i in range(name_count):
        start = locate(struct.unpack_from('<I', dll, name_array+i*4)[0])
        export_names.append(dll[start:dll.index(0, start)].decode())
    assert {'SKSEPlugin_Load', 'SKSEPlugin_Version'} <= set(export_names)
    print(json.dumps(dict(package=str(package), bytes=package.stat().st_size,
        sha256=digest(package.read_bytes()), dll_bytes=len(dll), dll_exports=export_names,
        source_commit=expected_commit, source_hashes_checked=len(info['source_sha256_lf']),
        plugin_bytes=len(plugin), all_checks='passed', in_game_tested=False), indent=2))
