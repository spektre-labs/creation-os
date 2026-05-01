#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
set -euo pipefail

echo "=== Ω-loop first REAL run ==="

# Ensure Qwen server is running
curl -sf http://localhost:8088/health || {
    echo "ERROR: Qwen server not running"
    echo "Start with: bash scripts/real/start_qwen_llama_server.sh"
    exit 1
}

# Environment
export COS_BITNET_SERVER_EXTERNAL=1
export COS_BITNET_SERVER_PORT=8088
export COS_OMEGA_SIM=0

# Clean omega state for fresh run
rm -rf ~/.cos/omega/
mkdir -p ~/.cos/omega/

# Run 5 real turns
./cos-omega --turns 5

# Report
./cos-omega --report

# Session artifacts
echo ""
echo "=== ARTIFACTS ==="
echo "Status:   ~/.cos/omega/status.txt"
echo "Ledger:   ~/.cos/state_ledger.json"
echo "Episodes: ~/.cos/engram_episodes.db"
echo "Receipts: ~/.cos/receipts/"
echo ""

# Print key metrics
echo "=== KEY METRICS ==="
cat ~/.cos/omega/status.txt 2>/dev/null || echo "(no status file)"

echo "=== First real run complete ==="
