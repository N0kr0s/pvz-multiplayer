#!/usr/bin/env bash
# Launch a native PvZ LAN test instance in its own Proton prefix.
set -euo pipefail
instance=${1:-client}
if [[ $instance != host && $instance != client ]]; then
    printf 'Usage: %s [host|client]\n' "$0" >&2
    exit 2
fi
mod_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
game_dir="$(cd -- "$mod_dir/.." && pwd)"
steam_dir="$(cd -- "$game_dir/../../.." && pwd)"
proton_path=${PVZMP_PROTON:-}
if [[ -z $proton_path ]]; then
    for candidate in "$steam_dir/steamapps/common/Proton 11.0/proton" \
        "$steam_dir/steamapps/common/Proton - Experimental/proton" \
        "$steam_dir/steamapps/common/Proton 9.0 (Beta)/proton"; do
        if [[ -f $candidate ]]; then proton_path=$candidate;break;fi
    done
fi
if [[ ! -f $proton_path ]]; then
    printf 'Proton not found. Set PVZMP_PROTON to the full path of a Proton proton script.\n' >&2
    exit 1
fi
for file in PlantsVsZombies.exe pvzgame.exe multiplayer/hook/pvzmp-loader.exe multiplayer/hook/pvzmp-hook.dll; do
    if [[ ! -f "$game_dir/$file" ]]; then printf 'Missing: %s\n' "$file" >&2;exit 1;fi
done
prefix_dir="$mod_dir/test-data/$instance"
mkdir -p "$prefix_dir"
# Keep test prefixes and single-instance mutexes separate from Steam's live profile.
export STEAM_COMPAT_DATA_PATH="$prefix_dir"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$steam_dir"
export SteamAppId=3590 SteamGameId=3590 PVZMP_TEST_INSTANCE="$instance"
export PROTON_LOG=1 PROTON_LOG_DIR="$prefix_dir"
printf 'Test instance: %s\nProton: %s\nSeparate profile: %s\n' "$instance" "$proton_path" "$prefix_dir"
cd -- "$game_dir"
exec "$proton_path" run "$game_dir/multiplayer/hook/pvzmp-loader.exe"
