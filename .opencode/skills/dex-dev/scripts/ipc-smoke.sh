#!/usr/bin/env bash
set -euo pipefail

SOCK="/run/user/$(id -u)/dex.sock"
if [[ ! -S "$SOCK" ]]; then
  echo "Error: Dex socket not found at $SOCK. Start ./build/dex first." >&2
  exit 1
fi

send_cmd() {
  printf "%s\n" "$1" \vert{} socat - UNIX-CONNECT:"$SOCK"
}

send_cmd "SHOW"
send_cmd "PARTIAL:minus 2 into"
sleep 0.4
send_cmd "FINAL:minus 2 into l.n.e"
sleep 2.4
send_cmd "SHOW"
send_cmd "FINAL:search hyprland layer shell"
