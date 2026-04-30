#!/usr/bin/env bash
# Long-running ultimate σ eval: activate sigma_gate_lsd venv, optional datasets install, run harness.
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_ultimate.sh: missing benchmarks/sigma_gate_lsd/.venv (create venv first)" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/ultimate_$(date +%Y%m%d_%H%M%S).log"

echo "=== sigma-gate ultimate eval: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

pip install datasets --quiet 2>&1 | tee -a "$LOG" || true

python3 benchmarks/sigma_gate_eval/run_ultimate_eval.py 2>&1 | tee -a "$LOG"

if [[ -f benchmarks/sigma_gate_eval/results_ultimate/ultimate_summary.json ]]; then
  python3 -m json.tool benchmarks/sigma_gate_eval/results_ultimate/ultimate_summary.json | tee -a "$LOG"
fi

git add python/cos/sigma_hide.py python/cos/sigma_spectral.py python/cos/sigma_ultimate.py python/cos/__init__.py benchmarks/sigma_gate_eval/ run_ultimate.sh 2>/dev/null || true
git commit -m "sigma-gate ultimate: LSD+HIDE+Spectral multi-signal $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
