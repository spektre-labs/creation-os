#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG="benchmarks/sigma_gate_eval/logs/integration_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"

echo "=== sigma-gate v4 INTEGRATION: $(date) ===" | tee "$LOG"

echo "=== Phase A: Gate smoke test ===" | tee -a "$LOG"
python3 -c "from cos.sigma_gate import SigmaGate; print('Gate import: OK')" 2>&1 | tee -a "$LOG"
python3 -c "
from cos.sigma_gate import SigmaGate
g = SigmaGate('${ROOT}/benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl')
print('Gate loaded. tau_accept=', g.tau_accept, 'tau_abstain=', g.tau_abstain)
print('sigma-gate v4 smoke: PASS')
" 2>&1 | tee -a "$LOG"

echo "=== Phase B: TruthfulQA eval (full 200; long) ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_eval.py 2>&1 | tee -a "$LOG"

echo "=== Results ===" | tee -a "$LOG"
python3 -m json.tool benchmarks/sigma_gate_eval/results/eval_summary.json 2>&1 | tee -a "$LOG" || true

if [[ "${GIT_INTEGRATION_COMMIT:-}" == "1" ]]; then
  git add python/cos benchmarks/sigma_gate_eval docs/reddit_ml_post_v2.md run_integration.sh Makefile README.md 2>/dev/null || true
  git commit -m "sigma-gate v4 integrated + eval $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "$LOG"
