
!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
# shellcheck source=/dev/null
if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
fi
export MPLBACKEND="${MPLBACKEND:-Agg}"
LOG="benchmarks/sigma_gate_eval/logs/cost_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"
echo "=== COST EVAL: $(date) ===" | tee "$LOG"
python3 benchmarks/sigma_gate_eval/run_cost_eval.py "$@" 2>&1 | tee -a "$LOG"
echo "=== COMPLETE: $(date) ===" | tee -a "$LOG"
