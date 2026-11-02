#!/usr/bin/env bash
# Publish the prepared source commit; no force push, game files or saves.
set -euo pipefail
release_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir=$(mktemp -d /tmp/pvzmp-publish.XXXXXX)
printf 'Source checkout: %s\n' "$repo_dir"
git clone --branch main "$release_dir/pvz-multiplayer-progress.bundle" "$repo_dir"
git -C "$repo_dir" remote set-url origin git@github.com:N0kr0s/pvz-multiplayer.git
# Avoid the system SSH config whose ownership/permissions caused an error here.
git -C "$repo_dir" -c core.sshCommand='ssh -F /dev/null' push -u origin main
