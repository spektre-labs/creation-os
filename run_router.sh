#!/usr/bin/env bash
# sigma-router tier distribution eval (holdout CSV).
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_router.sh: missing benchmarks/sigma_gate_lsd/.venv" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/router_$(date +%Y%m%d_%H%M%S).log"

echo "=== sigma-router eval: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

python3 benchmarks/sigma_gate_eval/run_router_eval.py 2>&1 | tee -a "$LOG"

if [[ -f benchmarks/sigma_gate_eval/results_router/router_summary.json ]]; then
  python3 -m json.tool benchmarks/sigma_gate_eval/results_router/router_summary.json | tee -a "$LOG"
fi

git add python/cos/sigma_router.py benchmarks/sigma_gate_eval/run_router_eval.py run_router.sh 2>/dev/null || true
git commit -m "sigma-router: tiered routing harness $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
