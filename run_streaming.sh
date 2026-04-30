#!/usr/bin/env bash
# Streaming per-token σ eval (greedy + optional early halt).
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_streaming.sh: missing benchmarks/sigma_gate_lsd/.venv" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/streaming_$(date +%Y%m%d_%H%M%S).log"

echo "=== streaming sigma-gate eval: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

python3 benchmarks/sigma_gate_eval/run_streaming_eval.py 2>&1 | tee -a "$LOG"

if [[ -f benchmarks/sigma_gate_eval/results_streaming/streaming_eval_summary.json ]]; then
  python3 -m json.tool benchmarks/sigma_gate_eval/results_streaming/streaming_eval_summary.json | tee -a "$LOG"
fi

git add python/cos/sigma_streaming.py benchmarks/sigma_gate_eval/run_streaming_eval.py run_streaming.sh 2>/dev/null || true
git commit -m "sigma-gate streaming: per-token halt scaffold eval $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
