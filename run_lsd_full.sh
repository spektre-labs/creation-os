#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate

LOG="benchmarks/sigma_gate_lsd/logs/lsd_full_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"

export MPLBACKEND=Agg
# Paper-scale defaults for this runner (do not inherit quick-test env from parent shell).
export LSD_NUM_PAIRS=1000
export LSD_EPOCHS=15
export LSD_BATCH_SIZE=32
export LSD_LEARNING_RATE=0.001
export LSD_MARGIN=0.3

echo "=== sigma-gate v4 FULL RUN: $(date) ===" | tee "$LOG"
echo "Pairs: $LSD_NUM_PAIRS, Epochs: $LSD_EPOCHS, batch=$LSD_BATCH_SIZE lr=$LSD_LEARNING_RATE margin=$LSD_MARGIN" | tee -a "$LOG"

echo "=== Phase 3: Full LSD reproduction ===" | tee -a "$LOG"
cd external/lsd
python3 -m lsd.main 2>&1 | tee -a "../../$LOG"
cd "$ROOT"

echo "=== Phase 3 results: ===" | tee -a "$LOG"
cat external/lsd/layerwise_semantic_dynamics_system/results/evaluation_results.json 2>/dev/null | tee -a "$LOG" || true

echo "=== Phase 4: Creation OS adaptation ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_lsd/adapt_lsd.py \
  --prompts benchmarks/truthfulqa200/prompts.csv \
  --output benchmarks/sigma_gate_lsd/results_full/ \
  --limit 200 \
  --epochs 15 \
  --min-pairs 800 \
  --batch-size 32 \
  --learning-rate 0.001 \
  --margin 0.3 \
  2>&1 | tee -a "$LOG"

echo "=== Phase 4 results: ===" | tee -a "$LOG"
cat benchmarks/sigma_gate_lsd/results_full/lsd_summary.json 2>/dev/null | tee -a "$LOG" || true

python3 -c "
import json
try:
    s = json.load(open('benchmarks/sigma_gate_lsd/results_full/lsd_summary.json'))
    auroc = float(s.get('cv_auroc_mean', 0) or 0)
    print('')
    print('  CV AUROC (mean): {:.4f}'.format(auroc))
    print('')
except Exception as e:
    print('Could not read summary:', e)
" 2>&1 | tee -a "$LOG"

if [[ "${GIT_LSD_FULL_COMMIT:-}" == "1" ]]; then
  git add benchmarks/sigma_gate_lsd/adapt_lsd.py benchmarks/sigma_probe_lsd/patches/sirraya_lsd_creation_os.patch run_lsd_full.sh 2>/dev/null || true
  git commit -m "sigma-gate v4 full: LSD 1000 pairs 15 epochs $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "$LOG"
