#!/usr/bin/env bash
# Creation OS v59 — σ-Budget microbench driver.
# Runs the three-point sweep (N = 64 / 512 / 4096) built into the binary.
set -euo pipefail
BIN="./creation_os_v59"
if [ ! -x "$BIN" ]; then
    echo "$BIN not built; run 'make standalone-v59' first" >&2
    exit 2
fi
"$BIN" --microbench
echo "microbench-v59: OK (3-point sweep completed; deterministic)"
