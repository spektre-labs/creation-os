#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
PROMPTS="${1:-benchmarks/truthfulqa200/prompts.csv}"
if [[ ! -f "$PROMPTS" ]]; then
  echo "error: missing $PROMPTS (run: python3 benchmarks/truthfulqa200/grade.py sample …)" >&2
  exit 2
fi
if [[ ! -x ./cos ]] && [[ ! -f ./cos ]]; then
  echo "error: run from repo root with ./cos present" >&2
  exit 2
fi
# Wave 6: full ``--multi-sigma`` (semantic σ); long wall clock per prompt.
# Defaults match grade.py; override in your shell if needed.
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
export COS_BITNET_IO_TIMEOUT_S="${COS_BITNET_IO_TIMEOUT_S:-600}"
export COS_TQA_CHAT_TIMEOUT="${COS_TQA_CHAT_TIMEOUT:-900}"
export COS_SEMANTIC_SIGMA_PARALLEL="${COS_SEMANTIC_SIGMA_PARALLEL:-0}"
exec python3 benchmarks/truthfulqa200/grade.py run \
  --cos ./cos \
  --prompts "$PROMPTS" \
  --results benchmarks/truthfulqa200/results.csv \
  "${@:2}"
