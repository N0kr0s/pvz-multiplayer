#!/usr/bin/env bash
set -euo pipefail
hook_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ ${SteamAppId:-} != 3590 ]]; then
    printf '%s\n' 'Run this wrapper from the original Plants vs. Zombies Steam entry (AppID 3590).' >&2
    exit 1
fi
# The widescreen release owns the root launcher and starts pvzgame.exe.
# Its game layout differs from Steam GOTY; leave the old injector out of this chain.
if [[ -f "$hook_dir/../widescreen-installed.json" && ! -f "$hook_dir/../wide-menu-enabled" ]]; then
    if [[ $# == 0 ]]; then
        printf '%s\n' 'Expected Steam %command%.' >&2
        exit 1
    fi
    cd -- "$hook_dir/../.."
    exec "$@"
fi
args=("$@")
replaced=0
for i in "${!args[@]}"; do
    if [[ ${args[i]##*/} == PlantsVsZombies.exe ]]; then
        args[i]="$hook_dir/pvzmp-loader.exe"
        replaced=$((replaced + 1))
    fi
done
if [[ $replaced != 1 || ${#args[@]} == 0 ]]; then
    printf '%s\n' 'Expected the original Steam command. Add steam-launch.sh before %command%.' >&2
    exit 1
fi
exec "${args[@]}"
