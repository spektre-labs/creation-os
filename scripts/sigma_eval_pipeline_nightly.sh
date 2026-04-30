#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
#
# Sequential σ eval harness: survives terminal disconnect when started with nohup(1).
# Requires HF_TOKEN or HUGGING_FACE_HUB_TOKEN (or HUGGINGFACE_HUB_TOKEN).
# Interpreter: CREATION_OS_PIPELINE_PYTHON, else REPO/.sigma_pipeline_venv/bin/python, else /usr/bin/python3.
# Logs under logs/pipeline_<stamp>/ — not intended for git commit.

set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO" || exit 1
# Prefer system Python so a broken repo-local .venv on PATH cannot hijack `python3`.
if [[ -n "${CREATION_OS_PIPELINE_PYTHON:-}" ]]; then
  PIPE_PY="${CREATION_OS_PIPELINE_PYTHON}"
elif [[ -x "${REPO}/.sigma_pipeline_venv/bin/python" ]]; then
  PIPE_PY="${REPO}/.sigma_pipeline_venv/bin/python"
elif [[ -x /usr/bin/python3 ]]; then
  PIPE_PY="/usr/bin/python3"
else
  PIPE_PY="$(command -v python3 2>/dev/null || echo python3)"
fi
export PYTHONPATH="${REPO}/python"
export MPLBACKEND="${MPLBACKEND:-Agg}"
export PYTHONUNBUFFERED="${PYTHONUNBUFFERED:-1}"

if [[ -z "${HF_TOKEN:-}" && -z "${HUGGING_FACE_HUB_TOKEN:-}" && -z "${HUGGINGFACE_HUB_TOKEN:-}" ]]; then
  echo "Missing HF_TOKEN / HUGGING_FACE_HUB_TOKEN / HUGGINGFACE_HUB_TOKEN; export one before starting." >&2
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
    cat <<EOF
# σ eval pipeline — run summary

- **Generated:** $(date -Iseconds)
- **Log directory:** ${LOGDIR}
- **Repo:** ${REPO}

## checkpoint.txt

EOF
    echo '```'
    cat "${LOGDIR}/checkpoint.txt" 2>/dev/null || echo "(empty)"
    echo '```'
    echo ""
    echo "## JSON snapshots (in this directory)"
    shopt -s nullglob 2>/dev/null || true
    for jf in "${LOGDIR}"/*.json; do
      echo "- $(basename "${jf}")"
    done
    echo ""
    for f in streaming_eval_summary.json router_summary.json gemma_hide_summary.json \
      halueval_oracle_summary.json halueval_v2_summary.json calibration_summary.json; do
      p="${LOGDIR}/${f}"
      if [[ -f "${p}" ]]; then
        echo "## ${f}"
        echo '```json'
        "${PIPE_PY}" -c "import json,sys; print(json.dumps(json.load(open(sys.argv[1])), indent=2))" "${p}" 2>/dev/null || cat "${p}"
        echo '```'
        echo ""
      fi
    done
    echo "## Per-step logs"
    echo "| Step | Log |"
    echo "|------|-----|"
    for name in streaming router gemma halueval_oracle halueval_v2 hide calibration cost cascade; do
      if [[ -f "${LOGDIR}/${name}.log" ]]; then
        echo "| ${name} | ${name}.log |"
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

log "pipeline dir=${LOGDIR} pid=$$ repo=${REPO} PIPE_PY=${PIPE_PY}"
checkpoint "START"

# Gemma: default 30 rows (v57); on Darwin use --cpu unless CREATION_OS_GEMMA_USE_CPU=0 (MPS OOM on 8GB).
_GEM_LIM="${CREATION_OS_GEMMA_LIMIT:-30}"
_GEM_EXTRA=()
if [[ "$(uname -s)" == "Darwin" && "${CREATION_OS_GEMMA_USE_CPU:-1}" != "0" ]]; then
  _GEM_EXTRA+=(--cpu)
fi
run_step streaming "${PIPE_PY}" benchmarks/sigma_gate_eval/run_streaming_eval.py \
  --holdout-limit 57 --max-new-tokens 100

run_step router "${PIPE_PY}" benchmarks/sigma_gate_eval/run_router_eval.py \
  --holdout-limit 0 --max-new-tokens 80 --calibrate-thresholds --p-fast 45

run_step gemma "${PIPE_PY}" benchmarks/sigma_gate_scaling/run_gemma_eval.py \
  --limit "${_GEM_LIM}" "${_GEM_EXTRA[@]}" --signals both --hide-layers "4,5,6,7,8,9,10"

run_step halueval_oracle "${PIPE_PY}" benchmarks/sigma_gate_eval/run_halueval_oracle_fix.py \
  --limit 80

run_step halueval_v2 "${PIPE_PY}" benchmarks/sigma_gate_eval/run_halueval_v2.py \
  --limit 200

run_step hide "${PIPE_PY}" benchmarks/sigma_gate_eval/run_hide_eval.py \
  --holdout-limit 0 --scan-limit 30 --trivia-limit 100 --halu-limit 100 --max-new-tokens 100

run_step calibration "${PIPE_PY}" benchmarks/sigma_gate_eval/run_calibration_eval.py \
  --limit 0 --max-new-tokens 80

run_step cost "${PIPE_PY}" benchmarks/sigma_gate_eval/run_cost_eval.py

run_step cascade "${PIPE_PY}" benchmarks/sigma_gate_eval/run_cascade_eval.py

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
  cp -f "${REPO}/benchmarks/sigma_gate_eval/results_halueval_v2/halueval_v2_summary.json" \
    "${LOGDIR}/" 2>/dev/null || true
} || true

checkpoint "END"
log "done; artefacts and per-step logs in ${LOGDIR}"
# write_report runs via trap EXIT
