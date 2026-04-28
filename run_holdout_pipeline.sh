#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG="benchmarks/sigma_gate_eval/logs/holdout_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"

echo "=== sigma-gate v4 FULL VALIDATION PIPELINE (holdout + cross-domain): $(date) ===" | tee "$LOG"

echo "=== Step 1: Create train/holdout splits ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_lsd/create_splits.py 2>&1 | tee -a "$LOG"

NTRAIN="$(python3 -c "import csv; import pathlib; p=pathlib.Path('benchmarks/sigma_gate_lsd/splits/train.csv'); print(sum(1 for _ in csv.DictReader(p.open())) )")"
echo "Train rows: ${NTRAIN}" | tee -a "$LOG"

echo "=== Step 2: Retrain on train split only ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_lsd/adapt_lsd.py \
  --prompts benchmarks/sigma_gate_lsd/splits/train.csv \
  --output benchmarks/sigma_gate_lsd/results_holdout/ \
  --limit "${NTRAIN}" \
  --epochs 15 \
  --min-pairs 600 \
  2>&1 | tee -a "$LOG"

echo "=== Step 3: Holdout evaluation ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_holdout_eval.py 2>&1 | tee -a "$LOG"

echo "=== HOLDOUT RESULTS ===" | tee -a "$LOG"
python3 -c "
import json
from pathlib import Path
p = Path('benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json')
s = json.loads(p.read_text(encoding='utf-8'))
auroc = s.get('auroc_wrong_vs_sigma', float('nan'))
cv = s.get('training_cv_auroc_manifest', float('nan'))
print('')
print('  HOLDOUT AUROC (wrong vs sigma): {:.4f}'.format(auroc))
print('  Training CV AUROC (manifest):   {:.4f}'.format(cv))
print('')
" 2>&1 | tee -a "$LOG"

echo "=== Step 4: Cross-domain evaluation (TriviaQA + HaluEval, no retrain) ===" | tee -a "$LOG"
pip install -q datasets sentence-transformers 2>&1 | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_cross_domain.py 2>&1 | tee -a "$LOG"

echo "=== CROSS-DOMAIN SUMMARY ===" | tee -a "$LOG"
if [[ -f benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json ]]; then
  cat benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json | tee -a "$LOG"
else
  echo "(cross_domain_summary.json missing)" | tee -a "$LOG"
fi

echo "=== FULL VALIDATION BANNER ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/print_full_validation_banner.py --full 2>&1 | tee -a "$LOG"

if [[ "${GIT_HOLDOUT_COMMIT:-}" == "1" ]]; then
  git add benchmarks/sigma_gate_lsd/create_splits.py benchmarks/sigma_gate_lsd/splits/ \
    benchmarks/sigma_gate_eval/run_holdout_eval.py benchmarks/sigma_gate_eval/run_cross_model.py \
    run_holdout_pipeline.sh .gitignore 2>/dev/null || true
  git commit -m "sigma-gate v4 holdout pipeline $(date +%Y-%m-%d)" || true
fi

if [[ "${GIT_FULL_VALIDATION_COMMIT:-}" == "1" ]]; then
  git add benchmarks/sigma_gate_eval/run_cross_domain.py \
    benchmarks/sigma_gate_eval/print_full_validation_banner.py \
    run_holdout_pipeline.sh Makefile .gitignore docs/sigma_gate_v4_publication_results.md README.md \
    2>/dev/null || true
  git commit -m "sigma-gate v4 full validation: holdout + cross-domain $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "$LOG"
