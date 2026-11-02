#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
g++ -std=c++20 -O2 -Wall -Wextra native.cpp -o pvzmp
exec ./pvzmp "$@"
