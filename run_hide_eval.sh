#!/usr/bin/env bash
# HIDE-only eval: optional clone of upstream reference repo, then run layer scan + benchmarks.
set -euo pipefail

REPO="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO"

if [[ -f benchmarks/sigma_gate_lsd/.venv/bin/activate ]]; then
  # shellcheck source=/dev/null
  source benchmarks/sigma_gate_lsd/.venv/bin/activate
else
  echo "run_hide_eval.sh: missing benchmarks/sigma_gate_lsd/.venv" >&2
  exit 2
fi

export PYTHONPATH="${REPO}/python${PYTHONPATH:+:${PYTHONPATH}}"
export MPLBACKEND=Agg

mkdir -p benchmarks/sigma_gate_eval/logs
LOG="benchmarks/sigma_gate_eval/logs/hide_eval_$(date +%Y%m%d_%H%M%S).log"

echo "=== HIDE eval: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee "$LOG"

if [[ ! -d external/hide/.git ]]; then
  mkdir -p external
  git clone --depth 1 https://github.com/C-anwoy/HIDE.git external/hide 2>&1 | tee -a "$LOG"
fi

pip install keybert datasets --quiet 2>&1 | tee -a "$LOG" || true

python3 benchmarks/sigma_gate_eval/run_hide_eval.py 2>&1 | tee -a "$LOG"

if [[ -f benchmarks/sigma_gate_eval/results_hide/hide_summary.json ]]; then
  python3 -m json.tool benchmarks/sigma_gate_eval/results_hide/hide_summary.json | tee -a "$LOG"
fi

git add python/cos/hide_metric_upstream.py python/cos/sigma_hide.py python/cos/sigma_ultimate.py \
  benchmarks/sigma_gate_eval/run_hide_eval.py benchmarks/sigma_gate_eval/run_ultimate_eval.py \
  run_hide_eval.sh 2>/dev/null || true
git commit -m "HIDE integration: upstream HSIC path + hide eval $(date +%Y-%m-%d)" 2>/dev/null || true

echo "=== complete: $(date -u +%Y-%m-%dT%H:%M:%SZ) ===" | tee -a "$LOG"
