#!/usr/bin/env bash
# Kutsuu yläkansion start_genesis (yksi totuus).
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
if [[ $# -eq 0 ]]; then
  exec "$HERE/../start_genesis"
else
  exec "$HERE/../start_genesis" "$@"
fi
