#!/usr/bin/env bash
# Creation OS v60 — σ-Shield microbench dispatcher.
#
# Runs the driver's built-in microbench (three-point sweep over batch
# sizes) and prints the raw output.  All measurements are deterministic
# (LCG-seeded requests, fixed iteration counts).

set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_v60
if [ ! -x "$BIN" ]; then
    echo "v60 microbench: building creation_os_v60 first..."
    make -s standalone-v60
fi

echo "=== Creation OS v60 — σ-Shield microbench ==="
"$BIN" --microbench
