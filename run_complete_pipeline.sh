#!/usr/bin/env bash
# Three-layer sigma pipeline eval (precheck + streaming + optional LSD post).
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_complete_pipeline.sh: missing benchmarks/sigma_gate_lsd/.venv" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/complete_$(date +%Y%m%d_%H%M%S).log"

echo "=== complete sigma-gate pipeline: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

PROBE="benchmarks/sigma_gate_lsd/results_holdout/sigma_gate_lsd.pkl"
if [[ -f "$REPO/$PROBE" ]]; then
  python3 benchmarks/sigma_gate_eval/run_complete_pipeline_eval.py --post-probe "$PROBE" 2>&1 | tee -a "$LOG"
else
  python3 benchmarks/sigma_gate_eval/run_complete_pipeline_eval.py 2>&1 | tee -a "$LOG"
fi

if [[ -f benchmarks/sigma_gate_eval/results_complete/complete_pipeline_summary.json ]]; then
  python3 -m json.tool benchmarks/sigma_gate_eval/results_complete/complete_pipeline_summary.json | tee -a "$LOG"
fi

git add python/cos/sigma_gate_precheck.py python/cos/sigma_precheck.py python/cos/sigma_gate_complete.py \
  benchmarks/sigma_gate_eval/run_complete_pipeline_eval.py run_complete_pipeline.sh 2>/dev/null || true
git commit -m "sigma-gate complete pipeline: precheck streaming post $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
