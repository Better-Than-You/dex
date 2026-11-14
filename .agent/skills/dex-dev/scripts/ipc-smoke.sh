#!/usr/bin/env bash
set -euo pipefail

RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
SOCK="$RUNTIME_DIR/dex.sock"

if [[ ! -S "$SOCK" ]]; then
    echo "Error: Dex socket not found at $SOCK. Start ./build/dex first." >&2
    exit 1
fi

send_cmd() {
    printf '%s\n' "$1" | socat - "UNIX-CONNECT:$SOCK"
}

echo "-> SHOW"
send_cmd "SHOW"

echo "-> PARTIAL:minus 2 into"
send_cmd "PARTIAL:minus 2 into"
sleep 0.4

echo "-> FINAL:minus 2 into l.n.e"
send_cmd "FINAL:minus 2 into l.n.e"
sleep 2.4

echo "-> FINAL:search hyprland layer shell"
send_cmd "FINAL:search hyprland layer shell"
sleep 2.4

echo "-> FINAL:dance for me (no match)"
send_cmd "FINAL:dance for me"
sleep 2.4

echo "-> HIDE"
send_cmd "HIDE"

echo "Done. Send QUIT to stop the daemon."
