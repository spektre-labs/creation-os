#!/usr/bin/env bash
# Replay lm-eval samples JSONL through ./cos chat (σ-gate) and write CSV + summary.
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Prereq: lm-eval output with --log_samples under benchmarks/lm_eval/<task>/
#          (see scripts/real/run_lm_eval_gemma_ollama.sh).
#
# Usage:
#   SAMPLES_GLOB='benchmarks/lm_eval/truthfulqa/local-completions/samples_truthfulqa_mc2_*.jsonl' \
#   RESULTS_GLOB='benchmarks/lm_eval/truthfulqa/local-completions/results_*.json' \
#   ./scripts/real/benchmark_gemma3.sh
#
# Env:
#   SAMPLES_GLOB   required glob for samples_*.jsonl
#   RESULTS_GLOB   optional aggregated results JSON from same lm-eval run
#   OUT_CSV        default benchmarks/lm_eval/coschat_replay.csv
#   LIMIT          optional max rows (quick smoke)
#   COS_BIN        default ./cos
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ -z "${SAMPLES_GLOB:-}" ]]; then
  echo "error: set SAMPLES_GLOB to samples_*.jsonl from lm-eval --log_samples" >&2
  exit 2
fi

OUT_CSV="${OUT_CSV:-benchmarks/lm_eval/coschat_replay.csv}"
RESULTS_ARG=()
if [[ -n "${RESULTS_GLOB:-}" ]]; then
  RESULTS_ARG=(--results-json "$RESULTS_GLOB")
fi

LIMIT_ARG=()
if [[ -n "${LIMIT:-}" ]]; then
  LIMIT_ARG=(--limit "$LIMIT")
fi

python3 scripts/real/benchmark_gemma3.py \
  --samples "$SAMPLES_GLOB" \
  "${RESULTS_ARG[@]}" \
  --out-csv "$OUT_CSV" \
  --cos-bin "${COS_BIN:-./cos}" \
  "${LIMIT_ARG[@]}"
