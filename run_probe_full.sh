#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# σ-probe v3 full run (100 prompts, crash-proof logging).
# Repo-root: same layout as creation-os; paths are relative to git root.
#
# Env:
#   SIGMA_PROBE_HF_MODEL   — default google/gemma-2-2b-it (set google/gemma-3-4b-it if RAM allows)
#   SIGMA_PROBE_DEVICE_MAP — default 1 on Darwin (requires accelerate + torch MPS/CUDA path via transformers)
#   SIGMA_PROBE_FULL_PIP   — set to 0 to skip pip install line

set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "${ROOT}" ]]; then
  ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
fi
cd "${ROOT}"

export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:${PATH}"
PYTHON="${PYTHON:-$(command -v python3)}"

LOG_DIR="${ROOT}/benchmarks/sigma_probe/logs"
OUT_DIR="${ROOT}/benchmarks/sigma_probe/results_full"
mkdir -p "${LOG_DIR}" "${OUT_DIR}"

LOG="${LOG_DIR}/probe_full_$(date +%Y%m%d_%H%M%S).log"

{
  echo "=== σ-PROBE FULL RUN: $(date -Iseconds) ==="
  echo "ROOT=${ROOT}"
  echo "disk:"
  df -h "${HOME}" 2>/dev/null || df -h .

  if [[ "${SIGMA_PROBE_FULL_PIP:-1}" != "0" ]]; then
    echo "=== pip install ==="
    "${PYTHON}" -m pip install -q transformers accelerate sentence-transformers scikit-learn torch --break-system-packages
  fi

  export SIGMA_PROBE_HF_MODEL="${SIGMA_PROBE_HF_MODEL:-google/gemma-2-2b-it}"
  if [[ "$(uname -s)" == "Darwin" ]] && [[ "${SIGMA_PROBE_DEVICE_MAP:-}" == "" ]]; then
    export SIGMA_PROBE_DEVICE_MAP=1
  fi

  # google/gemma-* on the Hub is gated; require a token unless user already picked another id.
  if [[ "${SIGMA_PROBE_HF_MODEL}" == google/gemma* ]] && [[ -z "${HF_TOKEN:-}${HUGGING_FACE_HUB_TOKEN:-}" ]]; then
    export SIGMA_PROBE_HF_MODEL="${SIGMA_PROBE_FALLBACK_MODEL:-TinyLlama/TinyLlama-1.1B-Chat-v1.0}"
    echo "WARN: Gemma is gated on Hugging Face (401 without HF_TOKEN). Using fallback: ${SIGMA_PROBE_HF_MODEL}"
    echo "      For real Gemma: export HF_TOKEN=... (see https://huggingface.co/settings/tokens ) then"
    echo "      SIGMA_PROBE_HF_MODEL=google/gemma-2-2b-it bash run_probe_full.sh"
  fi

  echo "SIGMA_PROBE_HF_MODEL=${SIGMA_PROBE_HF_MODEL}"
  echo "SIGMA_PROBE_DEVICE_MAP=${SIGMA_PROBE_DEVICE_MAP:-0}"

  echo "=== train_probe.py --limit 100 ==="
  "${PYTHON}" "${ROOT}/benchmarks/sigma_probe/train_probe.py" \
    --model gemma3_4b \
    --hf-model "${SIGMA_PROBE_HF_MODEL}" \
    --prompts "${ROOT}/benchmarks/truthfulqa200/prompts.csv" \
    --output "${OUT_DIR}/" \
    --limit 100

  SUM="${OUT_DIR}/probe_summary.json"
  if [[ -f "${SUM}" ]]; then
    AUROC_LINE="$("${PYTHON}" -c "import json; s=json.load(open('${SUM}')); print(s.get('cv_auroc_mean','nan'))" 2>/dev/null || echo nan)"
    echo "=== probe_summary CV AUROC (mean): ${AUROC_LINE} ==="
  fi

  if [[ "${SIGMA_PROBE_GIT_COMMIT:-1}" == "1" ]]; then
    git -C "${ROOT}" add "${ROOT}/run_probe_full.sh" 2>/dev/null || true
    git -C "${ROOT}" add -f "${OUT_DIR}/probe_summary.json" "${OUT_DIR}/probe_detail.jsonl" "${OUT_DIR}/sigma_probe.pkl" 2>/dev/null || true
    if ! git -C "${ROOT}" diff --cached --quiet 2>/dev/null; then
      MSG="sigma-probe v3 full: CV AUROC=${AUROC_LINE:-unknown}"
      git -C "${ROOT}" commit -m "${MSG}" || true
    else
      echo "git: nothing to commit"
    fi
  fi

  echo "=== COMPLETE: $(date -Iseconds) ==="
  if [[ -f "${SUM}" ]]; then
    "${PYTHON}" - <<'PY'
import json
from pathlib import Path
p = Path("benchmarks/sigma_probe/results_full/probe_summary.json")
if not p.is_file():
    raise SystemExit(0)
s = json.loads(p.read_text(encoding="utf-8"))
a = s.get("cv_auroc_mean")
b = s.get("cv_auroc_std")
t = s.get("train_auroc")
print("")
print(f"  CV AUROC: {a} ± {b}")
print(f"  Train AUROC: {t}")
print("")
if isinstance(a, (int, float)) and a >= 0.85:
    print("  ★ SHIP IT — σ-gate v3 works (diagnostic; validate repro)")
elif isinstance(a, (int, float)) and a >= 0.70:
    print("  ✓ PROMISING — optimize further")
elif isinstance(a, (int, float)) and a >= 0.60:
    print("  ~ MARGINAL — try more features")
else:
    print("  ✗ NEED DIFFERENT APPROACH or more data")
PY
  fi
} 2>&1 | tee -a "${LOG}"

echo "Log: ${LOG}"
