#!/usr/bin/env bash
# Multi-signal σ-gate lab runner: optional LapEigvals clone + eval.
# Set CREATION_MULTI_SIGNAL_COMMIT=1 to attempt a git commit at the end.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG_DIR="${ROOT}/benchmarks/sigma_gate_eval/logs"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/multi_signal_$(date +%Y%m%d_%H%M%S).log"

echo "=== MULTI-SIGNAL σ-GATE: $(date) ===" | tee "${LOG}"

if [[ ! -d "${ROOT}/external/lapeig" ]]; then
  echo "--- Clone LapEigvals (reference upstream) ---" | tee -a "${LOG}"
  mkdir -p "${ROOT}/external"
  git clone --depth 1 https://github.com/graphml-lab-pwr/lapeig.git "${ROOT}/external/lapeig" 2>&1 | tee -a "${LOG}"
else
  echo "--- external/lapeig already present ---" | tee -a "${LOG}"
fi

echo "--- run_multi_signal_eval.py ---" | tee -a "${LOG}"
python3 benchmarks/sigma_gate_eval/run_multi_signal_eval.py 2>&1 | tee -a "${LOG}"

if [[ -f benchmarks/sigma_gate_eval/results_multi_signal/multi_signal_summary.json ]]; then
  echo "--- summary ---" | tee -a "${LOG}"
  cat benchmarks/sigma_gate_eval/results_multi_signal/multi_signal_summary.json | tee -a "${LOG}"
fi

if [[ "${CREATION_MULTI_SIGNAL_COMMIT:-0}" == "1" ]]; then
  git add python/cos/sigma_spectral.py python/cos/sigma_unified.py \
    benchmarks/sigma_gate_eval/run_multi_signal_eval.py \
    benchmarks/sigma_gate_eval/results_multi_signal/ \
    Makefile .gitignore run_multi_signal.sh 2>/dev/null || true
  git commit -m "σ-gate multi-signal: LSD + spectral + unified $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "${LOG}"
