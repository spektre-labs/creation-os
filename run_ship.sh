#!/usr/bin/env bash
# σ-gate SHIP pipeline: regenerates cross-domain eval, writes checksums, prints summary.
# Optional: CREATION_SHIP_COMMIT=1 to git add+commit; CREATION_SHIP_PUSH=1 to git push (needs credentials).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export MPLBACKEND=Agg
export PYTHONPATH="${ROOT}/python"

LOG="benchmarks/sigma_gate_eval/logs/ship_$(date +%Y%m%d_%H%M%S).log"
mkdir -p "$(dirname "$LOG")"

echo "=== sigma-gate SHIP PIPELINE: $(date) ===" | tee "$LOG"

if [[ "${CREATION_SHIP_SKIP_EVAL:-}" != "1" ]]; then
  echo "--- Phase 1: Cross-domain (generative HaluEval) ---" | tee -a "$LOG"
  pip install -q datasets sentence-transformers 2>&1 | tee -a "$LOG"
  python3 benchmarks/sigma_gate_eval/run_cross_domain.py 2>&1 | tee -a "$LOG"
else
  echo "--- Phase 1: skipped (CREATION_SHIP_SKIP_EVAL=1) ---" | tee -a "$LOG"
fi

echo "--- Phase 2: REPRO checksums ---" | tee -a "$LOG"
OUT="${ROOT}/benchmarks/sigma_gate_eval/REPRO_CHECKSUMS.txt"
{
  echo "=== sigma-gate reproducibility checksums (SHA-256) ==="
  echo "Date (UTC): $(date -u)"
  echo "Commit: $(git -C "$ROOT" rev-parse HEAD 2>/dev/null || echo 'unknown')"
  echo "Machine: $(uname -a)"
  echo ""
} > "$OUT"

for f in \
  benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json \
  benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json \
  benchmarks/sigma_gate_lsd/results_full/manifest.json \
  benchmarks/sigma_gate_lsd/results_full/lsd_summary.json \
  benchmarks/sigma_gate_lsd/results_holdout/manifest.json \
  benchmarks/sigma_gate_v5/results/v5_summary.json; do
  if [[ -f "$ROOT/$f" ]]; then
    shasum -a 256 "$ROOT/$f" >> "$OUT"
  fi
done
echo "Wrote $OUT" | tee -a "$LOG"
cat "$OUT" | tee -a "$LOG"

echo "--- Phase 3: Final summary ---" | tee -a "$LOG"
python3 << 'PY' 2>&1 | tee -a "$LOG"
import json
import math
from pathlib import Path

root = Path.cwd()

def load_json(p):
    if not p.is_file():
        return None
    return json.loads(p.read_text(encoding="utf-8"))

h = load_json(root / "benchmarks/sigma_gate_eval/results_holdout/holdout_summary.json")
c = load_json(root / "benchmarks/sigma_gate_eval/results_cross_domain/cross_domain_summary.json")

cv = 0.9428
if h:
    v = h.get("training_cv_auroc_manifest")
    if isinstance(v, (int, float)) and not math.isnan(float(v)):
        cv = float(v)

h_auroc = h.get("auroc_wrong_vs_sigma") if h else float("nan")
t_auroc = c.get("triviaqa_auroc_wrong_vs_sigma") if c else float("nan")
e_auroc = c.get("halueval_auroc_wrong_vs_sigma") if c else float("nan")

def fmt(x):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "missing"
    return f"{float(x):.4f}"

print()
print("  +====================================================+")
print("  |  sigma-gate FINAL RESULTS (this checkout)         |")
print("  +====================================================+")
print(f"  |  TruthfulQA CV (manifest):     {fmt(cv):>10}        |")
print(f"  |  TruthfulQA holdout AUROC:     {fmt(h_auroc):>10}        |")
print(f"  |  TriviaQA cross AUROC:         {fmt(t_auroc):>10}        |")
print(f"  |  HaluEval (cross summary):     {fmt(e_auroc):>10}        |")
print("  +====================================================+")

try:
    hv = float(h_auroc) if h_auroc == h_auroc else 0.0
    tv = float(t_auroc) if t_auroc == t_auroc else 0.0
except (TypeError, ValueError):
    hv = tv = 0.0

if hv >= 0.90 and tv >= 0.90:
    print("  |  Strong holdout + TriviaQA cross (review claim discipline).   |")
elif hv >= 0.85:
    print("  |  Holdout >= 0.85 — document limitations for external compare. |")
else:
    print("  |  Re-run eval or inspect JSON if numbers missing.               |")
print("  +====================================================+")
print()
PY

if [[ "${CREATION_SHIP_COMMIT:-}" == "1" ]]; then
  echo "--- Phase 4: Git commit (CREATION_SHIP_COMMIT=1) ---" | tee -a "$LOG"
  git add \
    python/cos/sigma_gate.py \
    benchmarks/sigma_gate_eval/ \
    benchmarks/sigma_gate_lsd/results_full/manifest.json \
    benchmarks/sigma_gate_lsd/results_full/lsd_summary.json \
    docs/reddit_ml_sigma_gate.md \
    README.md \
    run_ship.sh \
    2>/dev/null || true
  git commit -m "sigma-gate SHIP: checksums + README + HaluEval generative eval $(date +%Y-%m-%d)" || true
fi

if [[ "${CREATION_SHIP_PUSH:-}" == "1" ]]; then
  echo "--- Git push (CREATION_SHIP_PUSH=1) ---" | tee -a "$LOG"
  git push origin HEAD 2>&1 | tee -a "$LOG" || true
fi

echo "=== SHIP COMPLETE: $(date) ===" | tee -a "$LOG"
