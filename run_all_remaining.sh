#!/usr/bin/env bash
# Run remaining σ-gate harness scripts (HIDE, multi-signal, ultimate, comparison merge).
# Does not push to remotes. Clones reference repos only if missing.
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_all_remaining.sh: missing benchmarks/sigma_gate_lsd/.venv" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/all_remaining_$(date +%Y%m%d_%H%M%S).log"

echo "=== all remaining evals: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

mkdir -p external
if [[ ! -d external/hide/.git ]]; then
  git clone --depth 1 https://github.com/C-anwoy/HIDE.git external/hide 2>&1 | tee -a "$LOG" || true
fi
if [[ ! -d external/hami/.git ]]; then
  git clone --depth 1 https://github.com/mala-lab/HaMI.git external/hami 2>&1 | tee -a "$LOG" || true
fi

pip install datasets keybert --quiet 2>&1 | tee -a "$LOG" || true

echo "=== HIDE eval ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_hide_eval.py 2>&1 | tee -a "$LOG" || echo "[skip] HIDE eval failed" | tee -a "$LOG"

echo "=== multi-signal eval ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_multi_signal_eval.py 2>&1 | tee -a "$LOG" || echo "[skip] multi-signal eval failed" | tee -a "$LOG"

echo "=== ultimate eval ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/run_ultimate_eval.py 2>&1 | tee -a "$LOG" || echo "[skip] ultimate eval failed" | tee -a "$LOG"

echo "=== comparison table ===" | tee -a "$LOG"
python3 benchmarks/sigma_gate_eval/create_comparison_table.py 2>&1 | tee -a "$LOG" || echo "[skip] comparison table failed" | tee -a "$LOG"

git add python/cos benchmarks/sigma_gate_hami benchmarks/sigma_gate_eval/run_hide_eval.py \
  benchmarks/sigma_gate_eval/run_ultimate_eval.py benchmarks/sigma_gate_eval/create_comparison_table.py \
  run_all_remaining.sh run_hide_eval.sh run_ultimate.sh .gitignore Makefile 2>/dev/null || true
git commit -m "sigma-gate harness: remaining evals + comparison merge $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
