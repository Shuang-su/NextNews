#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
cd "$NEXTNEWS_ROOT/apps/harmonyos"
exec devecocli build "$@"
