#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
#
# Sequential σ eval harness: survives terminal disconnect when started with nohup(1).
# Requires HF_TOKEN or HUGGINGFACE_HUB_TOKEN in the environment (export before launch).
# Logs under logs/pipeline_<stamp>/ — not intended for git commit.

set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO" || exit 1
export PYTHONPATH="${REPO}/python"
export MPLBACKEND="${MPLBACKEND:-Agg}"

if [[ -z "${HF_TOKEN:-}" && -z "${HUGGINGFACE_HUB_TOKEN:-}" ]]; then
  echo "Missing HF_TOKEN and HUGGINGFACE_HUB_TOKEN; export one before starting." >&2
  exit 1
fi

STAMP="$(date +%Y%m%d_%H%M%S)"
LOGDIR="${REPO}/logs/pipeline_${STAMP}"
mkdir -p "$LOGDIR"
echo "$$" >"${LOGDIR}/pipeline.pid"
ln -sfn "$LOGDIR" "${REPO}/logs/LATEST_PIPELINE"

log() { echo "[$(date -Iseconds)] $*" | tee -a "${LOGDIR}/pipeline.log"; }

checkpoint() {
  echo "$*" >>"${LOGDIR}/checkpoint.txt"
  log "$*"
}

write_report() {
  local r="${LOGDIR}/REPORT_SUMMARY.md"
  {
    echo "# σ eval pipeline — run summary"
    echo ""
    echo "- **Generated:** $(date -Iseconds)"
    echo "- **Log directory:** \`${LOGDIR}\`"
    echo "- **Repo:** \`${REPO}\`"
    echo ""
    echo "## checkpoint.txt"
    echo '```'
    cat "${LOGDIR}/checkpoint.txt" 2>/dev/null || echo "(empty)"
    echo '```'
    echo ""
    echo "## JSON snapshots (in this directory)"
    ls -1 "${LOGDIR}"/*.json 2>/dev/null | while read -r f; do echo "- \`$(basename "$f")\`"; done || true
    echo ""
    for f in streaming_eval_summary.json router_summary.json gemma_hide_summary.json \
      halueval_oracle_summary.json calibration_summary.json; do
      local p="${LOGDIR}/${f}"
      if [[ -f "${p}" ]]; then
        echo "## ${f}"
        echo '```json'
        python3 -c "import json,sys; print(json.dumps(json.load(open(sys.argv[1])), indent=2))" "${p}" 2>/dev/null || cat "${p}"
        echo '```'
        echo ""
      fi
    done
    echo "## Per-step logs"
    echo "| Step | Log |"
    echo "|------|-----|"
    for name in streaming router gemma halueval_oracle hide calibration cost cascade; do
      if [[ -f "${LOGDIR}/${name}.log" ]]; then
        echo "| ${name} | \`${name}.log\` |"
      fi
    done
  } >"${r}" 2>/dev/null || true
  log "wrote ${r}"
}

trap 'write_report' EXIT

run_step() {
  local name="$1"
  shift
  checkpoint "BEGIN ${name}"
  (cd "$REPO" && "$@") >>"${LOGDIR}/${name}.log" 2>&1
  local ec=$?
  if [[ "${ec}" -eq 0 ]]; then
    checkpoint "PASS ${name}"
  else
    checkpoint "FAIL ${name} exit=${ec}"
  fi
}

log "pipeline dir=${LOGDIR} pid=$$ repo=${REPO}"
checkpoint "START"

# --- eval steps (continue on failure) ---
run_step streaming python3 benchmarks/sigma_gate_eval/run_streaming_eval.py \
  --holdout-limit 20 --max-new-tokens 80

run_step router python3 benchmarks/sigma_gate_eval/run_router_eval.py \
  --holdout-limit 20 --max-new-tokens 40

run_step gemma python3 benchmarks/sigma_gate_scaling/run_gemma_eval.py \
  --limit 10

run_step halueval_oracle python3 benchmarks/sigma_gate_eval/run_halueval_oracle_fix.py \
  --limit 40

run_step hide python3 benchmarks/sigma_gate_eval/run_hide_eval.py \
  --holdout-limit 15 --scan-limit 15 --trivia-limit 20 --halu-limit 20 --max-new-tokens 40

run_step calibration python3 benchmarks/sigma_gate_eval/run_calibration_eval.py \
  --limit 40 --max-new-tokens 60

run_step cost python3 benchmarks/sigma_gate_eval/run_cost_eval.py

run_step cascade python3 benchmarks/sigma_gate_eval/run_cascade_eval.py

# --- snapshot JSON artefacts into log dir (best-effort) ---
{
  cp -f "${REPO}/benchmarks/sigma_gate_eval/results_streaming/streaming_eval_summary.json" \
    "${LOGDIR}/" 2>/dev/null || true
  cp -f "${REPO}/benchmarks/sigma_gate_eval/results_router/router_summary.json" \
    "${LOGDIR}/" 2>/dev/null || true
  cp -f "${REPO}/benchmarks/sigma_gate_scaling/results_gemma/gemma_hide_summary.json" \
    "${LOGDIR}/" 2>/dev/null || true
  cp -f "${REPO}/benchmarks/sigma_gate_eval/results_halueval_oracle/halueval_oracle_summary.json" \
    "${LOGDIR}/" 2>/dev/null || true
} || true

checkpoint "END"
log "done; artefacts and per-step logs in ${LOGDIR}"
# write_report runs via trap EXIT
