#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Crash-proof σ ablation driver: survives terminal disconnect (run under nohup or launch_ablation_detached.sh).
# See benchmarks/sigma_ablation/results/README.md — "Overnight crash-proof run".
#
# Environment (optional):
#   SIGMA_ABLATION_GIT_COMMIT=1   — git commit result artifacts (default: 1)
#   SIGMA_ABLATION_COMMIT_DETAIL=1 — include sigma_ablation_detail.jsonl (large; default: 0)
#   SIGMA_ABLATION_CAFFEINATE=0     — disable macOS caffeinate (default: on when caffeinate exists)
#   COS_ABLATION_HTTP_RETRIES=8    — transient Ollama HTTP / connection retries per completion

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "${LOG_DIR}"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
LOG="${LOG_DIR}/ablation_${TIMESTAMP}.log"
PID_FILE="${LOG_DIR}/ablation_runner.pid"

cleanup() {
  rm -f "${PID_FILE}"
}
trap cleanup EXIT

exec >>"${LOG}" 2>&1

echo "=== sigma-ablation START: $(date "+%Y-%m-%d %H:%M:%S %Z") ==="
echo "REPO_ROOT=${REPO_ROOT}"
echo "runner PID: $$"

if ! command -v curl >/dev/null 2>&1; then
  echo "ERROR: curl not found (needed for Ollama preflight)."
  exit 1
fi

if ! curl -sf "http://127.0.0.1:11434/api/tags" >/dev/null; then
  echo "ERROR: Ollama not reachable at http://127.0.0.1:11434/api/tags"
  echo "Start with: ollama serve   (and pull models, e.g. ollama pull gemma3:4b)"
  exit 1
fi

echo "$$" >"${PID_FILE}"

cd "${REPO_ROOT}"

if command -v ollama >/dev/null 2>&1; then
  echo "=== ollama list (first lines) ==="
  ollama list 2>/dev/null | head -20 || true
fi

PYTHON="${PYTHON:-python3}"
# Ease Metal VRAM pressure on small GPUs (override anytime).
export COS_ABLATION_MAX_TOKENS="${COS_ABLATION_MAX_TOKENS:-128}"

run_phase() {
  if [ "${SIGMA_ABLATION_CAFFEINATE:-1}" != "0" ] && [ "$(uname -s)" = "Darwin" ] && command -v caffeinate >/dev/null 2>&1; then
    caffeinate -dimsu -- "$@"
  else
    "$@"
  fi
}

echo "=== Phase 1: run_sigma_ablation.py --full ==="
run_phase "${PYTHON}" benchmarks/sigma_ablation/run_sigma_ablation.py \
  --config benchmarks/sigma_ablation/sigma_ablation_config.json \
  --full

echo "=== Phase 2: analyze_sigma_ablation.py --plots ==="
run_phase "${PYTHON}" benchmarks/sigma_ablation/analyze_sigma_ablation.py --plots

echo "=== Phase 3: optional git commit / push ==="
GIT_COMMIT="${SIGMA_ABLATION_GIT_COMMIT:-1}"
GIT_PUSH="${SIGMA_ABLATION_GIT_PUSH:-0}"
COMMIT_DETAIL="${SIGMA_ABLATION_COMMIT_DETAIL:-0}"

if [ "${GIT_COMMIT}" = "1" ]; then
  cd "${REPO_ROOT}"
  git add -f benchmarks/sigma_ablation/results/sigma_ablation_summary.json \
    benchmarks/sigma_ablation/results/sigma_ablation_table.md 2>/dev/null || true
  git add -f benchmarks/sigma_ablation/results/sigma_roc.png \
    benchmarks/sigma_ablation/results/sigma_hist_correct_wrong.png 2>/dev/null || true
  if [ "${COMMIT_DETAIL}" = "1" ]; then
    git add -f benchmarks/sigma_ablation/results/sigma_ablation_detail.jsonl 2>/dev/null || true
  fi
  if git diff --cached --quiet 2>/dev/null; then
    echo "No staged result changes (files missing, unchanged, or still gitignored without -f path)."
  else
    git commit -m "sigma-ablation results ${TIMESTAMP}" || echo "git commit: skipped or failed (non-fatal)"
    if [ "${GIT_PUSH}" = "1" ]; then
      git push origin HEAD || echo "git push: failed (non-fatal)"
    else
      echo "SIGMA_ABLATION_GIT_PUSH not set to 1 — skipping push."
    fi
  fi
else
  echo "SIGMA_ABLATION_GIT_COMMIT=0 — skipping git."
fi

trap - EXIT
rm -f "${PID_FILE}"
echo "=== sigma-ablation COMPLETE: $(date "+%Y-%m-%d %H:%M:%S %Z") ==="
echo "Log file: ${LOG}"
echo "Artifacts: benchmarks/sigma_ablation/results/"
