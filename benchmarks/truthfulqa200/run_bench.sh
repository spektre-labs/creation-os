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
# Ollama + logprobs + Codex system prompt often exceed the bitnet client default
# 60 s socket budget; without this, rows tend to time out and become all-ABSTAIN.
export COS_BITNET_IO_TIMEOUT_S="${COS_BITNET_IO_TIMEOUT_S:-240}"
export COS_TQA_CHAT_TIMEOUT="${COS_TQA_CHAT_TIMEOUT:-900}"
exec python3 benchmarks/truthfulqa200/grade.py run \
  --cos ./cos \
  --prompts "$PROMPTS" \
  --results benchmarks/truthfulqa200/results.csv \
  "${@:2}"
