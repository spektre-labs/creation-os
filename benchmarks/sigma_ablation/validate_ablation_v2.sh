#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Post-run checks for σ-ablation v2 (detail JSONL + summary + table).
# Usage: bash benchmarks/sigma_ablation/validate_ablation_v2.sh
# Or from repo root: make validate-sigma-ablation-v2

set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "${ROOT}" ]]; then
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
fi
export ROOT

DET="${ROOT}/benchmarks/sigma_ablation/results/sigma_ablation_detail.jsonl"
SUM="${ROOT}/benchmarks/sigma_ablation/results/sigma_ablation_summary.json"
TAB="${ROOT}/benchmarks/sigma_ablation/results/sigma_ablation_table.md"

if [[ ! -f "${DET}" ]]; then
  echo "FAIL: missing detail JSONL: ${DET}"
  echo "Run: make sigma-ablation-analyze (after run_sigma_ablation.py) or copy a detail file into results/."
  exit 2
fi

echo "=== σ-ablation v2 validation ==="
echo "ROOT=${ROOT}"

ROWS=$(wc -l <"${DET}" 2>/dev/null || echo 0)
echo "Detail rows: ${ROWS}"
if [[ "${ROWS}" -gt 100 ]]; then
  echo "  OK: sufficient data (>100 lines)"
else
  echo "  WARN: few rows (expected after a full run)"
fi

if [[ -f "${SUM}" ]]; then
  echo "  OK: summary exists"
else
  echo "  FAIL: missing ${SUM}"
  exit 2
fi

if [[ -f "${TAB}" ]]; then
  echo "  OK: table exists"
else
  echo "  FAIL: missing ${TAB}"
  exit 2
fi

python3 <<'PY'
import json
import os
import sys

root = os.environ["ROOT"]
det = os.path.join(root, "benchmarks/sigma_ablation/results/sigma_ablation_detail.jsonl")
sum_path = os.path.join(root, "benchmarks/sigma_ablation/results/sigma_ablation_summary.json")

if not os.path.isfile(det):
    print("  FAIL: detail missing", file=sys.stderr)
    sys.exit(2)

models = set()
signals = set()
mct = 0
seu = 0
with open(det, encoding="utf-8") as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        r = json.loads(line)
        models.add(r.get("model", ""))
        signals.add(r.get("signal", ""))
        ts = str(r.get("temp_strategy", "")).lower()
        if "mct" in ts:
            mct += 1
        sig = str(r.get("signal", "")).lower()
        if "seu" in sig:
            seu += 1

print(f"Models in detail: {len(models)}")
if len(models) >= 2:
    print("  OK: multi-model")
else:
    print("  WARN: fewer than 2 models")

print(f"Distinct signals: {len(signals)}")
if len(signals) >= 5:
    print("  OK: multi-signal")
else:
    print("  WARN: fewer than 5 signal names")

print(f"MCT-related rows (temp_strategy): {mct}")
if mct > 0:
    print("  OK: MCT present")
else:
    print("  WARN: no MCT rows")

print(f"SEU-related rows (signal contains 'seu'): {seu}")
if seu > 0:
    print("  OK: SEU present")
else:
    print("  WARN: no SEU rows")

with open(sum_path, encoding="utf-8") as f:
    s = json.load(f)

cdict = s.get("conclusions_dict") or s.get("conclusions_structured") or {}
best = cdict.get("best_overall")
if not isinstance(best, dict) or not best:
    best = s.get("best_overall") or {}
auroc = best.get("auroc") if isinstance(best, dict) else None
print(f"Best AUROC (summary): {auroc}")
try:
    a = float(auroc)
except (TypeError, ValueError):
    a = 0.0
if a >= 0.75:
    print("  NOTE: AUROC >= 0.75 — competitive band (diagnostic only)")
elif a >= 0.65:
    print("  NOTE: AUROC >= 0.65 — separation worth deeper validation")
elif a >= 0.55:
    print("  NOTE: AUROC in marginal band")
else:
    print("  NOTE: AUROC below 0.55 on best arm — boundary / non-separating plausible")
PY

echo "=== validation complete ==="
