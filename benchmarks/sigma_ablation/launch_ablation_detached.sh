#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Launch σ overnight ablation fully detached from the controlling terminal.
# Use this (or the documented nohup one-liner) so the run survives Cursor quit.
#
# Refuses to start if ablation_master.pid still points to a live process.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
LOG_DIR="${SCRIPT_DIR}/logs"
mkdir -p "${LOG_DIR}"
MASTER_PID_FILE="${LOG_DIR}/ablation_master.pid"
NOHUP_LOG="${LOG_DIR}/nohup_ablation.log"
DRIVER="${SCRIPT_DIR}/run_ablation_overnight.sh"

if [ -f "${MASTER_PID_FILE}" ]; then
  old="$(tr -d ' \n' <"${MASTER_PID_FILE}" || true)"
  if [ -n "${old}" ] && ps -p "${old}" >/dev/null 2>&1; then
    echo "refuse: ablation_master.pid=${old} is still running (one driver at a time)." >&2
    exit 1
  fi
fi

cd "${REPO_ROOT}"
if command -v setsid >/dev/null 2>&1; then
  # New session: no controlling tty; survives IDE terminal teardown.
  nohup setsid bash "${DRIVER}" >>"${NOHUP_LOG}" 2>&1 &
else
  nohup bash "${DRIVER}" >>"${NOHUP_LOG}" 2>&1 &
fi
echo $! >"${MASTER_PID_FILE}"
echo "sigma-ablation-launched: master_pid=$(cat "${MASTER_PID_FILE}")"
echo "log (wrapper): ${NOHUP_LOG}"
echo "driver log:    ${LOG_DIR}/ablation_*.log (timestamped after start)"
