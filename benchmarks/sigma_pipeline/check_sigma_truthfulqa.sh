#!/usr/bin/env bash
#
# Smoke test for FIX-5: TruthfulQA pipeline evaluation.
#
# Runs the Creation OS σ-pipeline over a 50-question TruthfulQA
# sample (bundled under benchmarks/sigma_pipeline/fixtures/) and
# pins the deterministic stub-backend behaviour:
#
#   n == 50
#   accuracy_local    = 30% (StubBackend's baseline truth rate)
#   accuracy_hybrid   = 96% (σ-gate correctly escalates uncertain)
#   accuracy_retained = 1.00 (hybrid matches always-API on TQA shape)
#
# This is not an accuracy claim about a real model — it measures
# σ-gate behaviour on a real-world misconception benchmark.  A real
# BitNet / cloud evaluation replaces the StubBackend but reuses the
# same harness and fixture layout.
#
# The full 817-question run requires lm-eval-harness + weights; see
# benchmarks/truthfulqa_sigma.sh for the external-model path.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

FIXTURE="benchmarks/sigma_pipeline/fixtures/truthfulqa_sample.jsonl"
[[ -f "$FIXTURE" ]] || { echo "missing fixture $FIXTURE" >&2; exit 2; }

# Line count must match 50 — gives us a canonical N the numbers
# below are pinned against.
LINES=$(wc -l < "$FIXTURE" | tr -d ' ')
if [[ "$LINES" -ne 50 ]]; then
    echo "FAIL fixture has $LINES lines, expected 50" >&2; exit 3
fi

OUT_JSON="$(mktemp)"
trap 'rm -f "$OUT_JSON"' EXIT

PYTHONPATH=scripts python3 -m sigma_pipeline.pipeline_bench \
    --input "$FIXTURE" \
    --output-md /dev/null \
    --output-json "$OUT_JSON" >/dev/null

echo "--- TruthfulQA sample run (50 questions) ---"
cat "$OUT_JSON"

# ---- pin the five headline numbers -------------------------------
grep -q '"n": 50'                           "$OUT_JSON" || { echo "FAIL n"; exit 4; }
grep -q '"n_escalated": 50'                 "$OUT_JSON" || { echo "FAIL escalation"; exit 5; }
grep -q '"accuracy_api": 0.96'              "$OUT_JSON" || { echo "FAIL accuracy_api"; exit 6; }
grep -q '"accuracy_local": 0.3'             "$OUT_JSON" || { echo "FAIL accuracy_local"; exit 7; }
grep -q '"accuracy_hybrid": 0.96'           "$OUT_JSON" || { echo "FAIL accuracy_hybrid"; exit 8; }
grep -q '"accuracy_retained": 1.0'          "$OUT_JSON" || { echo "FAIL accuracy_retained"; exit 9; }
grep -q '"local_share": 0.0'                "$OUT_JSON" || { echo "FAIL local_share"; exit 10; }

echo "check-sigma-truthfulqa: PASS (50Q pipeline, local=30% → hybrid=96% via σ-escalation)"
