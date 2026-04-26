#!/usr/bin/env bash
# TruthfulQA MC1 through ./cos chat with σ-gate (ABSTAIN counts as truthful).
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Prerequisites: `make cos`, Ollama (or BitNet server) on COS_BITNET_SERVER_PORT.
#
# Data: place benchmarks/truthfulqa/mc_task.json or let this script download from GitHub.
#   GitHub list length is ~790; HuggingFace `validation` has 817 rows — same MC1 schema.
#
# Environment:
#   TRUTHFULQA_N   — max prompts (default 100). Set to 817 (or 0 for all) for full run.
#   TRUTHFULQA_MC_JSON — override path to mc_task.json
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="${OUTDIR:-benchmarks/truthfulqa}"
mkdir -p "$OUTDIR"

MC_JSON="${TRUTHFULQA_MC_JSON:-$OUTDIR/mc_task.json}"
N_RUN="${TRUTHFULQA_N:-100}"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"

if [[ ! -x ./cos ]]; then
  echo "error: build ./cos first (make cos)" >&2
  exit 1
fi

if [[ ! -f "$MC_JSON" ]]; then
  echo "Downloading TruthfulQA mc_task.json …"
  mkdir -p "$(dirname "$MC_JSON")"
  if ! curl -fsSL "https://raw.githubusercontent.com/sylinrl/TruthfulQA/main/data/mc_task.json" -o "$MC_JSON"; then
    echo "error: curl failed. Install data manually or use HuggingFace:" >&2
    echo "  python3 -c \"from datasets import load_dataset; import json; d=load_dataset('truthfulqa/truthful_qa','multiple_choice'); json.dump(list(d['validation']), open('$MC_JSON','w'))\"" >&2
    exit 1
  fi
fi

export TRUTHFULQA_ROOT="$ROOT"
RESULT_CSV="$OUTDIR/truthfulqa_results.csv"
PROMPTS_CSV="$OUTDIR/prompts_preview.csv"

python3 "$ROOT/scripts/real/truthfulqa_sigma_run.py" \
  --json "$MC_JSON" \
  --out-csv "$RESULT_CSV" \
  --prompts-csv "$PROMPTS_CSV" \
  --n "$N_RUN" \
  --cos "./cos"

echo ""
echo "Wrote $RESULT_CSV"
echo "Wrote $PROMPTS_CSV"
