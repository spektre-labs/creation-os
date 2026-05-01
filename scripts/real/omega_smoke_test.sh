#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
set -euo pipefail

echo "=== Ω-loop smoke test ==="

# Clean state
rm -rf ~/.cos/omega/
mkdir -p ~/.cos/omega/

# Run 10 turns in simulation mode
COS_OMEGA_SIM=1 ./cos-omega --turns 10

# Check outputs exist
test -f ~/.cos/omega/status.txt && echo "PASS: status.txt exists" || echo "FAIL: no status.txt"

# Print report
./cos-omega --report

echo "=== Smoke test complete ==="
