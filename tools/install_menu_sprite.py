#!/usr/bin/env python3
"""Register the generated PNG as a native PopCap resource."""
from pathlib import Path
import sys,struct
sys.path.insert(0,str(Path(__file__).resolve().parent))
from compact_menu import unpack,ROOT

def main():
    path=ROOT/'main.pak';original=path.read_bytes()
    backup=ROOT/'multiplayer/backups/compact-menu-before-png.pak'
    if not backup.exists():backup.write_bytes(original)
    entries=unpack(original);output=[]
    for name,meta,data in entries:
        if name.lower()=='properties\\resources.xml':
            xml=data.decode('utf-8-sig')
            if 'id="MP_PLAQUE"' not in xml:
                needle='<Image id="EDITBOX" path="editbox" />'
                assert xml.count(needle)==1
                xml=xml.replace(needle,'''<SetDefaults path="multiplayer/assets" idprefix="IMAGE_" />
<Image id="MP_PLAQUE" path="multiplayer-stone" />
<SetDefaults path="images" idprefix="IMAGE_" />
'''+needle)
            if 'id="MP_LABEL"' not in xml:
                xml=xml.replace('<Image id="MP_PLAQUE" path="multiplayer-stone" />',
                    '<Image id="MP_PLAQUE" path="multiplayer-stone" />\n<Image id="MP_LABEL" path="multiplayer-label" />')
            data=xml.encode()
        output.append((name,meta,data))
    header=bytearray(bytes.fromhex('c04ac0ba00000000'))
    for name,meta,data in output:
        encoded=name.encode();header+=bytes([0,len(encoded)])+encoded+struct.pack('<I',len(data))+meta
    header+=b'\x80'
    packed=bytes(x^247 for x in header+b''.join(data for _,_,data in output))
    assert len(unpack(packed))==len(entries)
    path.write_bytes(packed)
    print('Registered IMAGE_MP_PLAQUE in LoadingImages. Existing entries retained.')

if __name__=='__main__':main()
