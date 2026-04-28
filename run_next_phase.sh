#!/usr/bin/env bash
# σ-gate next-phase lab runner (spectral smoke, cross-domain, optional unified eval).
# Optional commit: CREATION_NEXT_PHASE_COMMIT=1 bash run_next_phase.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG_DIR="${ROOT}/benchmarks/sigma_gate_eval/logs"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/next_phase_$(date +%Y%m%d_%H%M%S).log"

echo "=== σ-GATE NEXT PHASE: $(date) ===" | tee "${LOG}"

echo "--- 1. Cross-domain (HaluEval generative default) ---" | tee -a "${LOG}"
PYTHONPATH="${ROOT}/python" python3 benchmarks/sigma_gate_eval/run_cross_domain.py 2>&1 | tee -a "${LOG}"

echo "--- 2. Spectral σ smoke ---" | tee -a "${LOG}"
python3 -c "
from cos.sigma_spectral import spectral_sigma
from transformers import AutoModelForCausalLM, AutoTokenizer

model = AutoModelForCausalLM.from_pretrained(
    'openai-community/gpt2', output_attentions=True
)
tokenizer = AutoTokenizer.from_pretrained('openai-community/gpt2')
model.eval()

sigma, _ = spectral_sigma(
    model, tokenizer,
    'What is the capital of France?',
    'Paris is the capital of France.',
)
sigma2, _ = spectral_sigma(
    model, tokenizer,
    'What is the capital of France?',
    'The moon is made of cheese.',
)
print(f'Spectral σ (factual): {sigma:.4f}')
print(f'Spectral σ (odd):     {sigma2:.4f}')
print(f'Delta: {sigma2 - sigma:.4f}')
" 2>&1 | tee -a "${LOG}"

echo "--- 3. Unified gate holdout (if script present) ---" | tee -a "${LOG}"
if [[ -f benchmarks/sigma_gate_eval/run_unified_eval.py ]]; then
  python3 benchmarks/sigma_gate_eval/run_unified_eval.py 2>&1 | tee -a "${LOG}"
fi

if [[ "${CREATION_NEXT_PHASE_COMMIT:-0}" == "1" ]]; then
  echo "--- 4. Git commit (CREATION_NEXT_PHASE_COMMIT=1) ---" | tee -a "${LOG}"
  git add python/cos benchmarks/sigma_gate_eval benchmarks/sigma_gate_scaling Makefile .gitignore run_next_phase.sh 2>/dev/null || true
  git commit -m "σ-gate next phase: spectral + unified $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "${LOG}"
