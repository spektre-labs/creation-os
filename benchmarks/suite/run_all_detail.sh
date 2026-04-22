#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# BENCH-2 driver: materialise all four SCI-6 detail JSONLs via cos-chat.
set -euo pipefail
cd "$(dirname "$0")/../.."

# One global driver at a time (portable lock: ``flock`` is not on stock macOS).
SUITE_RUN_LOCK="${COS_SUITE_RUN_LOCK:-/tmp/creation_os_suite_run_all.lockdir}"
while ! mkdir "$SUITE_RUN_LOCK" 2>/dev/null; do
  echo "run_all_detail: waiting for lock $SUITE_RUN_LOCK …" >&2
  sleep 8
done
cleanup_lock() { rmdir "$SUITE_RUN_LOCK" 2>/dev/null || true; }
trap cleanup_lock EXIT INT TERM

: "${CREATION_OS_BITNET_EXE:?set CREATION_OS_BITNET_EXE}"
: "${CREATION_OS_BITNET_MODEL:?set CREATION_OS_BITNET_MODEL}"

# Speed: skip llama.cpp empty warmup each spawn; shorter completions for MC.
export CREATION_OS_BITNET_ARG0="${CREATION_OS_BITNET_ARG0:---no-warmup}"
export CREATION_OS_BITNET_N_PREDICT="${CREATION_OS_BITNET_N_PREDICT:-96}"

for ds in arc_challenge arc_easy gsm8k hellaswag; do
  echo "=== ${ds} ==="
  python3 scripts/real/run_suite_cos_chat_detail.py \
    --rows "benchmarks/suite/data/${ds}_rows.jsonl" \
    --out "benchmarks/suite/${ds}_detail.jsonl"
done

echo "=== cos-bench-suite-sci ==="
./creation_os_sigma_suite_sci --alpha 0.80 --delta 0.10 \
  --out benchmarks/suite/full_results.json
