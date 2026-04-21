#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Full-width TruthfulQA (generation / validation split, 817 rows by
# default) through sigma_pipeline.pipeline_bench.
#
# Environment:
#   TRUTHFULQA817_JSONL   input JSONL (default below)
#   COS_PIPELINE_BACKEND  stub | llama_cli  (default stub)
#
# llama_cli requires CREATION_OS_BITNET_EXE + CREATION_OS_BITNET_MODEL.
#
# Output JSON summary path:
#   TRUTHFULQA817_SUMMARY_JSON (default benchmarks/pipeline/truthfulqa_817.json)
#
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

JSONL="${TRUTHFULQA817_JSONL:-benchmarks/pipeline/data/truthfulqa_817.jsonl}"
OUT="${TRUTHFULQA817_SUMMARY_JSON:-benchmarks/pipeline/truthfulqa_817.json}"
BACKEND="${COS_PIPELINE_BACKEND:-stub}"

if [[ ! -f "$JSONL" ]]; then
    echo "Missing fixture: $JSONL"
    echo "Generate it with:"
    echo "  python3 scripts/real/export_truthfulqa_jsonl.py --split validation --out \"$JSONL\""
    exit 2
fi

N="$(wc -l < "$JSONL" | tr -d ' ')"
echo "pipeline_bench: backend=$BACKEND rows=$N"

PYTHONPATH=scripts python3 -m sigma_pipeline.pipeline_bench \
    --input "$JSONL" \
    --backend "$BACKEND" \
    --output-md /dev/null \
    --output-json "$OUT"

echo "Wrote summary -> $OUT"
