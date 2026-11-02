#!/usr/bin/env python3
"""Restore the verified Steam profile into the original-version widescreen profile."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'multiplayer/backups/saves-before-widescreen-migration/steam-cloud'
DEST = Path('/home/nokros/.local/share/Steam/steamapps/compatdata/3590/pfx/drive_c/ProgramData/PopCap Games/PlantsVsZombies/userdata')

def profile():
    original = (SOURCE / 'user1.dat').read_bytes()
    if len(original) != 1312 or struct.unpack_from('<I', original)[0] != 12:
        raise SystemExit('Unexpected source profile; no files changed.')
    version, level, coins, completed = struct.unpack_from('<4I', original)
    plants = struct.unpack_from('<I', original, 816)[0]
    # pvzgame.exe SyncDetails at 0x468390: 820-byte base, then 88 bytes per plant.
    if (level, coins, completed, plants) != (13, 1373, 1, 4):
        raise SystemExit('Source profile differs from the inspected save; no files changed.')
    end = 820 + 88 * plants
    return original[:end]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apply', action='store_true')
    args = parser.parse_args()
    converted = profile()
    print('Ready: Nokros, level 2-3, adventure completed once, coins 13730, Zen plants 4.')
    print('Target:', DEST)
    if not args.apply:
        print('Read-only check complete. Close PvZ and use --apply to restore.')
        return
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            comm = (proc / 'comm').read_text().strip().lower()
            status = (proc / 'status').read_text()
            if any(n in comm for n in ('pvzgame', 'plantsvszombie', 'popcapgame')) and '\nState:\tZ' not in status:
                raise SystemExit('Close PvZ before restoring. No files changed.')
        except (OSError, PermissionError):
            pass
    current = (DEST / 'user1.dat').read_bytes()
    if current == converted:
        print('Already restored. No files changed.')
        return
    if len(current) != 820 or struct.unpack_from('<3I', current, 4) != (1, 0, 0):
        raise SystemExit('The target is no longer a fresh profile. Refusing to overwrite new progress.')
    backup = ROOT / 'multiplayer/backups' / ('before-profile-restore-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))
    backup.mkdir(parents=True)
    for name in ('users.dat', 'user1.dat'):
        shutil.copy2(DEST / name, backup / name)
    for name, data in [('users.dat', (SOURCE / 'users.dat').read_bytes()), ('user1.dat', converted)]:
        temp = DEST / (name + '.pvzmp-restore-tmp')
        temp.write_bytes(data)
        temp.replace(DEST / name)
        if (DEST / name).read_bytes() != data:
            raise SystemExit('Verification failed. Backup: ' + str(backup))
    print('Profile restored and verified. Backup:', backup)

if __name__ == '__main__':
    main()
