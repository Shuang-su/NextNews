#!/usr/bin/env bash
# Foreground development server, reachable from the connected device over HDC only.
set -euo pipefail
source "$(dirname "$0")/env.sh"
stream_dir="${1:-$NEXTNEWS_ROOT/.local/sog-huafa}"
stream_port="${2:-${NEXTNEWS_STREAM_PORT:-8768}}"
[[ "$stream_port" =~ ^[0-9]+$ ]] || { echo 'Invalid port'; exit 1; }
[[ -f "$stream_dir/scene.json" ]] || { echo 'scene.json not found'; exit 1; }
if lsof -iTCP:"$stream_port" -sTCP:LISTEN -t >/dev/null 2>&1; then
  echo "Port $stream_port is already in use; keep the existing server or stop it first."; exit 1
fi
stream_hdc="$DEVECO_STUDIO_HOME/Contents/sdk/default/openharmony/toolchains/hdc"
"$stream_hdc" rport "tcp:$stream_port" "tcp:$stream_port"
if [[ "${3:-}" == "--background" ]]; then
  mkdir -p "$NEXTNEWS_ROOT/.local"
  python3 - "$stream_dir" "$stream_port" "$NEXTNEWS_ROOT/.local" <<'PYTHON'
import pathlib, subprocess, sys
root, port, local = pathlib.Path(sys.argv[1]).resolve(), sys.argv[2], pathlib.Path(sys.argv[3])
with (local / f'stream-server-{port}.log').open('ab') as log:
    child = subprocess.Popen([sys.executable, '-m', 'http.server', port, '--bind', '127.0.0.1', '--directory', str(root)],
                             stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
(local / f'stream-server-{port}.pid').write_text(str(child.pid))
print(f'Background server PID {child.pid}; loopback port {port}; log: {local / ("stream-server-" + port + ".log")}')
PYTHON
  exit 0
fi
trap '"$stream_hdc" fport rm "tcp:$stream_port" "tcp:$stream_port" >/dev/null 2>&1 || true' EXIT
python3 -m http.server "$stream_port" --bind 127.0.0.1 --directory "$stream_dir"
