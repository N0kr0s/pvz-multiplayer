#!/usr/bin/env bash
set -euo pipefail
# Run once in a terminal on the Linux HOST. Does not disable the firewall.
if [[ ${1:-} == --help ]]; then
    printf '%s\n' 'Usage: bash multiplayer/tools/allow-linux-host.sh [LAN_CIDR]' 'Default: detect the connected LAN subnet. Opens TCP 35900-35909 only from that subnet.'
    exit 0
fi
lan_subnet=${1:-}
if [[ -z $lan_subnet ]]; then
    lan_subnet=$(ip -4 route show scope link | awk '$1 ~ /^[0-9]+\./ && $0 !~ /docker|br-|tailscale|tun[0-9]|wg[0-9]/ {print $1; exit}')
fi
if [[ ! $lan_subnet =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}/[0-9]{1,2}$ ]]; then
    printf '%s\n' 'Could not detect LAN subnet. Specify it, for example: bash multiplayer/tools/allow-linux-host.sh 192.168.31.0/24' >&2
    exit 1
fi
command -v ufw >/dev/null || { printf '%s\n' 'UFW is not installed. No firewall changes made.' >&2; exit 1; }
printf 'Allow LAN %s -> TCP 35900-35909 (PvZ Multiplayer)\n' "$lan_subnet"
sudo ufw allow from "$lan_subnet" to any port 35900:35909 proto tcp comment 'PvZ Multiplayer LAN'
sudo ufw status numbered
