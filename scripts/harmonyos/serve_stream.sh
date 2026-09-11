#!/usr/bin/env bash
# Foreground development server, reachable from the connected device over HDC only.
set -euo pipefail
source "$(dirname "$0")/env.sh"
stream_dir="${1:-$NEXTNEWS_ROOT/.local/stream-huafa}"
stream_port="${NEXTNEWS_STREAM_PORT:-8767}"
[[ "$stream_port" =~ ^[0-9]+$ ]] || { echo 'Invalid port'; exit 1; }
[[ -f "$stream_dir/scene.json" ]] || { echo 'scene.json not found'; exit 1; }
if lsof -iTCP:"$stream_port" -sTCP:LISTEN -t >/dev/null 2>&1; then
  echo "Port $stream_port is already in use; keep the existing server or stop it first."; exit 1
fi
stream_hdc="$DEVECO_STUDIO_HOME/Contents/sdk/default/openharmony/toolchains/hdc"
"$stream_hdc" rport "tcp:$stream_port" "tcp:$stream_port"
trap '"$stream_hdc" fport rm "tcp:$stream_port" "tcp:$stream_port" >/dev/null 2>&1 || true' EXIT
python3 -m http.server "$stream_port" --bind 127.0.0.1 --directory "$stream_dir"
