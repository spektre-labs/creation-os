#!/usr/bin/env bash
# BREAK IT: adversarial σ-gate stress driver (see scripts/real/break_it_harness.py).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
TIMEOUT="${BREAK_IT_TIMEOUT:-120}"
export BREAK_IT_TIMEOUT="${BREAK_IT_TIMEOUT:-120}"
echo "=== BREAK IT stress suite ==="
echo "Model:   ${COS_BITNET_CHAT_MODEL}"
echo "Port:    ${COS_BITNET_SERVER_PORT}"
echo "Timeout: ${TIMEOUT}s per prompt (override with BREAK_IT_TIMEOUT)"
echo ""
exec python3 "${ROOT}/scripts/real/break_it_harness.py" "$@"
