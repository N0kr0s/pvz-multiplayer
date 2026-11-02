#!/usr/bin/env python3
"""Patch animation coordinates; retain original image assets and PAK timestamps."""
from pathlib import Path
import hashlib,json,struct,zlib
ROOT=Path(__file__).resolve().parents[2]
TARGET='compiled\\reanim\\SelectorScreen.reanim.compiled'
TRACKS={
 'SelectorScreen_Survival_button':173.1,
 'SelectorScreen_Survival_shadow':173.1,
 'SelectorScreen_Challenges_button':257.5,
 'SelectorScreen_Challenges_shadow':257.5,
 'SelectorScreen_ZenGarden_button':328.0,
 'SelectorScreen_ZenGarden_shadow':328.0,
}

def patch_compiled(data):
    magic,size=struct.unpack_from('<II',data)
    assert magic==0xdeadfed4
    raw=bytearray(zlib.decompress(data[8:]));assert len(raw)==size
    count=struct.unpack_from('<I',raw,8)[0]
    assert count==48
    assert all(struct.unpack_from('<I',raw,24+i*12+8)[0]==706 for i in range(48))
    report=[]
    for name,rest in TRACKS.items():
        encoded=name.encode();pos=raw.find(encoded)
        assert pos>=4 and raw.find(encoded,pos+1)==-1
        assert struct.unpack_from('<I',raw,pos-4)[0]==len(encoded)
        start=pos+len(encoded)
        assert struct.unpack_from('<I',raw,start)[0]==44
        start+=4
        # Each track owns 706 fixed-size transform records, then serialized strings.
        shift=.30*(rest-173.1)
        changes=0
        for frame in range(706):
            offset=start+44*frame
            y=struct.unpack_from('<f',raw,offset+4)[0]
            if y!=-10000:
                struct.pack_into('<f',raw,offset+4,y-shift);changes+=1
            scale=struct.unpack_from('<f',raw,offset+20)[0]
            if scale!=-10000: struct.pack_into('<f',raw,offset+20,scale*.70)
        # Default transform scale is 1 when the initial key uses the sentinel.
        if struct.unpack_from('<f',raw,start+20)[0]==-10000:
            struct.pack_into('<f',raw,start+20,.70)
        report.append({'track':name,'vertical_shift':shift,'keys':changes})
    return struct.pack('<II',magic,len(raw))+zlib.compress(raw,9),report

def unpack(data):
    raw=bytes(x^247 for x in data);p=8;entries=[]
    assert raw[:8]==bytes.fromhex('c04ac0ba00000000')
    while raw[p]==0:
        p+=1;n=raw[p];p+=1;name=raw[p:p+n].decode();p+=n
        size=struct.unpack_from('<I',raw,p)[0];meta=raw[p+4:p+12];p+=12
        entries.append([name,size,meta])
    p+=1;result=[]
    for name,size,meta in entries:
        result.append((name,meta,raw[p:p+size]));p+=size
    assert p==len(raw)
    return result

def main():
    backup=ROOT/'multiplayer/backups/widescreen-before-compact-menu.pak'
    current=ROOT/'main.pak'
    if not backup.exists(): backup.write_bytes(current.read_bytes())
    entries=unpack(backup.read_bytes());output=[];report=None
    for name,meta,data in entries:
        if name==TARGET: data,report=patch_compiled(data)
        output.append((name,meta,data))
    assert report
    header=bytearray(bytes.fromhex('c04ac0ba00000000'))
    for name,meta,data in output:
        encoded=name.encode();header+=bytes([0,len(encoded)])+encoded+struct.pack('<I',len(data))+meta
    header+=b'\x80'
    packed=bytes(x^247 for x in header+b''.join(data for _,_,data in output))
    verify=unpack(packed)
    assert len(verify)==len(entries)
    assert sum(a!=b for a,b in zip(entries,verify))==1
    current.write_bytes(packed)
    (ROOT/'multiplayer/compact-menu.json').write_text(json.dumps({'sha256':hashlib.sha256(packed).hexdigest(),'tracks':report},indent=2))
    print('Patched 6 animation tracks; all other PAK entries unchanged.')

if __name__=='__main__':main()
