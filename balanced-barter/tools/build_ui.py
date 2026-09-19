"""Build the single SkyUI 6.11 barter override using its pinned FFDec toolchain."""
from pathlib import Path
import re
import subprocess
import sys
import zlib

def functions(text):
    result={}
    for m in re.finditer(r"^   function (\w+)\([^\n]*\)\s*\{",text,re.M):
        start=m.start(); pos=m.end(); depth=1
        # Both inputs use balanced braces in strings/comments. Fail on mismatch.
        while depth and pos<len(text):
            depth+=(text[pos]=='{')-(text[pos]=='}'); pos+=1
        if depth: raise ValueError(f"Unclosed function {m[1]}")
        result[m[1]]=(start,pos,text[start:pos])
    return result

def merge(original, extension):
    methods=functions(extension)
    existing=functions(original)
    output=original
    for name,(start,end,_) in sorted(existing.items(),key=lambda x:x[1][0],reverse=True):
        if name in methods: output=output[:start]+methods[name][2]+output[end:]
    extras='\n'.join(value[2] for name,value in methods.items() if name not in existing)
    first=extension.index('   function ')
    variables=extension[extension.index('{')+1:first]
    output=output[:output.index('{')+1]+variables+output[output.index('{')+1:]
    end=output.rfind('}')
    output=output[:end]+extras+'\n'+output[end:]
    # No user selection is allowed to invoke the vanilla immediate transaction.
    assert 'GameDelegate.call("ItemSelect"' not in output
    return output

def run(*args):
    subprocess.run(list(map(str,args)),check=True)

def build(upstream, work, output):
    root=Path(__file__).resolve().parents[1]
    source=upstream/'source/actionscript'
    stage=work/'as/__Packages'; stage.mkdir(parents=True,exist_ok=True)
    selected=[*(f'Common/skyui/defines/{n}.as' for n in ['Actor','Armor','Form','Input','Inventory','Item','Magic','Material','Weapon']),
              *(f'Common/skyui/filter/{n}.as' for n in ['ItemTypeFilter','NameFilter','SortFilter']),
              *(f'ItemMenus/{n}.as' for n in ['BarterDataSetter','BarterMenu','BottomBar','CategoryList','InventoryDataSetter','InventoryIconSetter','InventoryLists','ItemMenu','ItemcardDataExtender']),
              'Vanilla/Components/Meter.as','Vanilla/Shared/GlobalFunc.as']
    versions={'BarterMenu.as','BottomBar.as','InventoryLists.as'}
    for name in selected:
        content=(source/name).read_text(encoding='utf-8-sig')
        relative=name.split('/',1)[1]
        if relative=='BarterMenu.as': content=merge(content,(root/'ui/BarterExtensions.as').read_text())
        if relative in versions:
            index=content.index('{')+1
            content=content[:index]+'\n   static var SKYUI_RELEASE_IDX = 2026;\n   static var SKYUI_VERSION_MAJOR = 6;\n   static var SKYUI_VERSION_MINOR = 11;\n   static var SKYUI_VERSION_STRING = "6.11 SE";\n'+content[index:]
        dest=stage/relative; dest.parent.mkdir(parents=True,exist_ok=True); dest.write_text(content,encoding='utf-8')
    ffdec=upstream/'tools/FFDec/ffdec-cli.exe'
    output.parent.mkdir(parents=True,exist_ok=True)
    base=work/'barter-base.swf'
    run(ffdec,'-xml2swf',upstream/'source/swf/bartermenu.xml',base)
    run(ffdec,'-config','autoDeobfuscate=false,decompile=false','-onerror','abort','-importScript',base,output,stage.parent)
    data=output.read_bytes()
    if data[:3]==b'CWS': data=data[:8]+zlib.decompress(data[8:])
    assert data[:3] in (b'CWS',b'FWS') and len(data)>20000
    for marker in [b'BalancedBarterPanel',b'BBResult',b'BBConfirm',b'Queue',b'6.11 SE']:
        assert marker in data, f'Missing compiled UI marker: {marker!r}'
    print(f'Built and verified {output} ({output.stat().st_size} bytes)')

if __name__=='__main__': build(*(Path(p).resolve() for p in sys.argv[1:]))
