#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Graded σ benchmark: prompts with known answers → graded_results.csv +
# graded_comparison.md (σ vs correctness, abstention, wrong+ACCEPT).
#
# Env:
#   COS_BITNET_*          (defaults: external Ollama 11434, gemma3:4b)
#   COS_ENGRAM_DISABLE    default 1
#   COS_GRADED_CHAT_TIMEOUT_S  per-prompt timeout (default 300)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"

if [[ ! -x ./cos ]]; then
  echo "error: ./cos missing — run 'make cos'" >&2
  exit 1
fi

python3 "$ROOT/scripts/real/graded_benchmark.py" "$@"
rc=$?
if [[ "$rc" -eq 0 ]] && [[ -f benchmarks/graded/graded_results.csv ]]; then
  python3 "$ROOT/scripts/real/compute_auroc.py" || true
  cp -f benchmarks/graded/graded_results.csv benchmarks/graded/graded_200_results.csv
fi
exit "$rc"
