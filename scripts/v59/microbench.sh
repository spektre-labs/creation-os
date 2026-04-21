#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
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
