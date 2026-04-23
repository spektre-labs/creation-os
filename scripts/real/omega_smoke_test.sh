#!/usr/bin/env bash
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
