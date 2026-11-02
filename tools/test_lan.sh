#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
flags=()
if [[ ${1:-} == --simulated ]]; then
    flags+=(-DPVZMP_LAN_TEST_NETWORK)
elif [[ $# != 0 ]]; then
    printf 'Usage: %s [--simulated]\n' "$0" >&2
    exit 2
fi
binary=$(mktemp /tmp/pvzmp-lan-test.XXXXXX)
trap 'rm -f "$binary"' EXIT
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pthread "${flags[@]}" \
    multiplayer/tools/test_lan.cpp multiplayer/hook/lan.cpp -o "$binary"
"$binary"
