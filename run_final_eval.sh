#!/usr/bin/env bash
# Full multi-signal + multi-benchmark lab run (see benchmarks/sigma_gate_eval/*).
# Optional git commit: CREATION_FINAL_EVAL_COMMIT=1 bash run_final_eval.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG_DIR="${ROOT}/benchmarks/sigma_gate_eval/logs"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/final_eval_$(date +%Y%m%d_%H%M%S).log"

echo "=== σ-GATE FINAL EVAL: $(date) ===" | tee "${LOG}"

pip install datasets --quiet 2>&1 | tee -a "${LOG}" || true

if [[ ! -d "${ROOT}/external/lapeig" ]]; then
  echo "--- Clone LapEigvals (reference only) ---" | tee -a "${LOG}"
  mkdir -p "${ROOT}/external"
  git clone --depth 1 https://github.com/graphml-lab-pwr/lapeig.git "${ROOT}/external/lapeig" 2>&1 | tee -a "${LOG}" || true
fi

echo "--- 1. Multi-signal holdout ---" | tee -a "${LOG}"
python3 benchmarks/sigma_gate_eval/run_multi_signal_eval.py 2>&1 | tee -a "${LOG}"

echo "--- 2. Cross-domain multi-benchmark ---" | tee -a "${LOG}"
python3 benchmarks/sigma_gate_eval/run_cross_domain_multi.py 2>&1 | tee -a "${LOG}"

echo "--- 3. Summary (JSON with auroc keys) ---" | tee -a "${LOG}"
python3 <<'PY' 2>&1 | tee -a "${LOG}"
import json
from pathlib import Path

root = Path("benchmarks/sigma_gate_eval")
paths = sorted(root.glob("results_*/*summary*.json")) + sorted(root.glob("results_*/*/*summary*.json"))
seen = set()
rows = []
for p in paths:
    rp = str(p.resolve())
    if rp in seen:
        continue
    seen.add(rp)
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        continue
    name = p.parent.name
    if isinstance(data, dict):
        for k, v in data.items():
            if "auroc" in k.lower() and isinstance(v, (int, float)):
                rows.append((name, k, float(v)))

print()
print("  sigma_gate_eval JSON summaries (AUROC-related keys)")
for name, k, v in rows[:80]:
    print(f"    {name:40s}  {k:42s}  {v:.4f}")
if len(rows) > 80:
    print(f"    ... ({len(rows) - 80} more lines)")
PY

if [[ "${CREATION_FINAL_EVAL_COMMIT:-0}" == "1" ]]; then
  git add python/cos benchmarks/sigma_gate_eval Makefile .gitignore run_final_eval.sh 2>/dev/null || true
  git commit -m "σ-gate final eval: multi-signal + multi-benchmark $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "${LOG}"
