#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
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
