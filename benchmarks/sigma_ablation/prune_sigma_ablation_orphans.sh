#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# After a Cursor / SSH disconnect, stray "cos chat --fast" or extra
# run_sigma_ablation.py processes may remain (parent PID 1).  This script
# removes those without touching the current overnight driver in
# logs/ablation_master.pid (if present).
#
# Safe to run while a nohup overnight job is active: the canonical Python
# child of ablation_master.pid is never killed.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
MASTER_PID_FILE="${SCRIPT_DIR}/logs/ablation_master.pid"
COS_PATTERN="cos chat --once --json --no-stream --fast --no-cache"

keep_python=""
if [ -f "${MASTER_PID_FILE}" ]; then
  mp="$(tr -d ' \n' <"${MASTER_PID_FILE}" || true)"
  if [ -n "${mp}" ] && ps -p "${mp}" >/dev/null 2>&1; then
    keep_python="$(pgrep -P "${mp}" | while read -r c; do ps -p "${c}" -o command= 2>/dev/null | grep -q run_sigma_ablation && echo "${c}"; done | head -1 || true)"
  fi
fi

killed_py=0
while read -r pid; do
  [ -z "${pid}" ] && continue
  [ -n "${keep_python}" ] && [ "${pid}" = "${keep_python}" ] && continue
  cmd="$(ps -p "${pid}" -o command= 2>/dev/null || true)"
  echo "${cmd}" | grep -q run_sigma_ablation || continue
  echo "${cmd}" | grep -q "${REPO_ROOT}" || continue
  echo "prune: SIGKILL run_sigma_ablation pid=${pid}"
  kill -9 "${pid}" 2>/dev/null || true
  killed_py=$((killed_py + 1))
done < <(pgrep -f "run_sigma_ablation.py" || true)

killed_cos=0
while read -r pid; do
  [ -z "${pid}" ] && continue
  pp="$(ps -o ppid= -p "${pid}" 2>/dev/null | tr -d ' ' || true)"
  [ "${pp}" != "1" ] && continue
  cmd="$(ps -p "${pid}" -o command= 2>/dev/null || true)"
  echo "${cmd}" | grep -qF "${COS_PATTERN}" || continue
  echo "${cmd}" | grep -qF "${REPO_ROOT}/cos" || continue
  echo "prune: SIGKILL orphan cos pid=${pid}"
  kill -9 "${pid}" 2>/dev/null || true
  killed_cos=$((killed_cos + 1))
done < <(pgrep -f "${COS_PATTERN}" || true)

echo "prune_sigma_ablation_orphans: done (python=${killed_py}, cos=${killed_cos})"
