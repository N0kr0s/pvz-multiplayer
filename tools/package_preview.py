#!/usr/bin/env python3
"""Package mod-owned sources/assets and built Windows binaries; never the game or profiles."""
from pathlib import Path
from zipfile import ZipFile,ZIP_DEFLATED
from datetime import datetime,timezone
import hashlib,json
ROOT=Path(__file__).resolve().parents[2]
EXCLUDE={'backups','releases','test-data','save-migration','__pycache__','extracted-assets'}
SUFFIXES={'.cpp','.h','.S','.sh','.cmd','.py','.js','.css','.html','.md','.png','.patch','.manifest'}
BINARIES=['multiplayer/start-windows.exe','multiplayer/hook/pvzmp-hook.dll','multiplayer/hook/pvzmp-loader.exe','multiplayer/install-menu.exe']
def main():
    files=[ROOT/n for n in ('.gitignore','README.md','deep-research-pvz.md') if (ROOT/n).is_file()]
    files += [p for p in (ROOT/'multiplayer').rglob('*') if p.is_file() and not(EXCLUDE&set(p.relative_to(ROOT).parts)) and (p.suffix in SUFFIXES or p.name=='wide-menu-enabled')]
    for n in BINARIES:
        p=ROOT/n
        if not p.is_file():raise SystemExit('Build first; missing '+n)
        files.append(p)
    manifest={'build':'native-gameplay-preview','protocol':13,'host_tcp_ports':[35900,35909],'created_utc':datetime.now(timezone.utc).isoformat(),
              'game_sha256':'5d8a46a2ba095c8539693234dc88da4c2c2ef901a404020721a49c5beaac0eab',
              'sha256':{n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in BINARIES}}
    target=ROOT/'multiplayer/releases/pvzmp-native-gameplay-preview.zip';target.parent.mkdir(exist_ok=True)
    temp=target.with_suffix('.zip.tmp')
    with ZipFile(temp,'w',ZIP_DEFLATED) as archive:
        for p in sorted(set(files)):archive.write(p,p.relative_to(ROOT).as_posix())
        archive.writestr('multiplayer/BUILD.json',json.dumps(manifest,indent=2)+'\n')
    with ZipFile(temp) as archive:
        assert archive.testzip() is None
        assert not any(n.endswith('.pak') or n.endswith('.dat') or '/backups/' in n or '/test-data/' in n for n in archive.namelist())
    temp.replace(target)
    print(target)
    print('SHA256:',hashlib.sha256(target.read_bytes()).hexdigest())
if __name__=='__main__':main()
