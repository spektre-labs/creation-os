#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
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
