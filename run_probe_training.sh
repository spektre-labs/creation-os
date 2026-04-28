#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Crash-proof σ-probe training driver (repo-root detection).
# Optional: SIGMA_PROBE_GIT_PUSH=1 to push after commit (default: no push).

set -euo pipefail

ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "${ROOT}" ]]; then
  ROOT="$(cd "$(dirname "$0")" && pwd)"
fi
cd "${ROOT}"

LOG_DIR="${ROOT}/benchmarks/sigma_probe/logs"
mkdir -p "${LOG_DIR}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG="${LOG_DIR}/probe_${TIMESTAMP}.log"

exec >>"${LOG}" 2>&1

echo "=== σ-probe training START: $(date -Iseconds) ==="
echo "ROOT=${ROOT}"

PYTHON="${PYTHON:-$(command -v python3)}"
export PATH="/opt/homebrew/bin:/usr/local/bin:/usr/bin:/bin:${PATH}"

if [[ "${SIGMA_PROBE_TRAIN_INSTALL:-0}" == "1" ]]; then
  echo "=== pip install (SIGMA_PROBE_TRAIN_INSTALL=1) ==="
  "${PYTHON}" -m pip install transformers accelerate sentence-transformers scikit-learn torch --break-system-packages
fi

echo "=== Training probe ==="
"${PYTHON}" "${ROOT}/benchmarks/sigma_probe/train_probe.py" \
  --model gemma3:4b \
  --prompts "${ROOT}/benchmarks/truthfulqa200/prompts.csv" \
  --output "${ROOT}/benchmarks/sigma_probe/results/" \
  --limit 100

GIT_COMMIT="${SIGMA_PROBE_GIT_COMMIT:-1}"
GIT_PUSH="${SIGMA_PROBE_GIT_PUSH:-0}"

if [[ "${GIT_COMMIT}" == "1" ]]; then
  git add "${ROOT}/benchmarks/sigma_probe/"*.py "${ROOT}/benchmarks/sigma_probe/results/"*.json \
    "${ROOT}/benchmarks/sigma_probe/results/"*.jsonl 2>/dev/null || true
  git add -f "${ROOT}/benchmarks/sigma_probe/results/"* 2>/dev/null || true
  if git diff --cached --quiet 2>/dev/null; then
    echo "git: nothing to commit"
  else
    git commit -m "σ-probe v3: hidden state trajectory training ${TIMESTAMP}" || echo "git commit skipped"
    if [[ "${GIT_PUSH}" == "1" ]]; then
      git push origin HEAD || echo "git push failed (non-fatal)"
    fi
  fi
fi

echo "=== σ-probe training COMPLETE: $(date -Iseconds) ==="
echo "Log: ${LOG}"
