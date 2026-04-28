#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG="benchmarks/sigma_gate_v5/logs/v5_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"

echo "=== sigma-GATE v5 START: $(date) ===" | tee "$LOG"

pip install -q datasets sentence-transformers 2>&1 | tee -a "$LOG"

python3 benchmarks/sigma_gate_v5/run_v5_pipeline.py 2>&1 | tee -a "$LOG"

if [[ "${CREATION_SIGMA_V5_COMMIT:-}" == "1" ]]; then
  git add benchmarks/sigma_gate_v5/ run_v5.sh benchmarks/sigma_gate_eval/semantic_labeling.py \
    benchmarks/sigma_gate_eval/run_holdout_eval.py benchmarks/sigma_gate_eval/run_cross_domain.py \
    benchmarks/sigma_probe/train_probe.py run_holdout_pipeline.sh .gitignore Makefile 2>/dev/null || true
  git commit -m "sigma-gate v5 multi-dataset lab pipeline $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "$LOG"
