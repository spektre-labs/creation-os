#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Same prompts.csv; run bench per model. Override MODELS="gemma3:4b phi4" etc.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
MODELS="${MODELS:-gemma3:4b phi4}"
for m in $MODELS; do
  echo "=== model: $m ==="
  safe="${m//:/_}"
  export COS_BITNET_CHAT_MODEL="$m"
  export COS_OLLAMA_MODEL="$m"
  bash benchmarks/truthfulqa200/run_bench.sh benchmarks/truthfulqa200/prompts.csv --fresh
  mv -f benchmarks/truthfulqa200/results.csv "benchmarks/truthfulqa200/results_${safe}.csv"
  echo "wrote results_${safe}.csv"
done
echo "Done. If a model OOMs, retry with MODELS='gemma3:4b llama3.2:3b' or document phi4-mini in RESULTS_MULTI.md."
