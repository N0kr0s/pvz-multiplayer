#!/usr/bin/env python3
"""Build entry-local XOR deltas. Shipping this patch requires the original PAK."""
from pathlib import Path
import struct
from compact_menu import ROOT,unpack

def fnv(data):
    value=2166136261
    for b in data:value=((value^b)*16777619)&0xffffffff
    return value

def main():
    original=unpack((ROOT/'multiplayer/backups/widescreen-before-compact-menu.pak').read_bytes())
    current=unpack((ROOT/'main.pak').read_bytes())
    assert [x[:2] for x in original]==[x[:2] for x in current]
    changes=[(a[0],a[2],b[2]) for a,b in zip(original,current) if a[2]!=b[2]]
    assert {x[0] for x in changes}=={'compiled\\reanim\\SelectorScreen.reanim.compiled','properties\\resources.xml'}
    result=bytearray(b'PVZMPK01'+struct.pack('<I',len(changes)))
    for name,old,new in changes:
        encoded=name.encode();length=max(len(old),len(new))
        delta=bytes(a^b for a,b in zip(old.ljust(length,b'\0'),new.ljust(length,b'\0')))
        result+=bytes([len(encoded)])+encoded+struct.pack('<IIII',len(old),fnv(old),len(new),fnv(new))+delta
    target=ROOT/'multiplayer/assets/menu.patch';target.write_bytes(result)
    print('Wrote',len(result),'bytes of XOR deltas for',len(changes),'PAK entries.')
if __name__=='__main__':main()
