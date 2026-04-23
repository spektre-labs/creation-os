#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Dual llama-server processes: draft (2B, fast) + verify (9B, quality).
# Tune via COS_BITNET_SERVER_EXE, COS_SPEC_DRAFT_* / COS_SPEC_VERIFY_* mirrors.
#
#   export COS_BITNET_SERVER_EXTERNAL=1
#   export COS_SPEC_DRAFT_PORT=8089 COS_SPEC_VERIFY_PORT=8088
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

EXE="${COS_BITNET_SERVER_EXE:-/opt/homebrew/bin/llama-server}"
DRAFT_MODEL="${COS_SPEC_DRAFT_MODEL_PATH:-$ROOT/models/qwen3.5-2b-Q4_K_M.gguf}"
VERIFY_MODEL="${COS_SPEC_VERIFY_MODEL_PATH:-$ROOT/models/qwen3.5-9b-Q4_K_M.gguf}"
if [[ ! -f "$VERIFY_MODEL" && -f "$ROOT/models/qwen3-8b-Q4_K_M.gguf" ]]; then
  VERIFY_MODEL="$ROOT/models/qwen3-8b-Q4_K_M.gguf"
fi

HOST="${COS_BITNET_SERVER_HOST:-127.0.0.1}"
DRAFT_PORT="${COS_SPEC_DRAFT_PORT:-8089}"
VERIFY_PORT="${COS_SPEC_VERIFY_PORT:-8088}"
CTX="${COS_BITNET_CHAT_CTX:-${COS_LLAMA_CTX:-2048}}"
BATCH="${COS_LLAMA_BATCH:-512}"
THREADS="${COS_LLAMA_THREADS:-2}"

for M in "$DRAFT_MODEL" "$VERIFY_MODEL"; do
  if [[ ! -f "$M" ]]; then
    echo "start_dual_server.sh: GGUF missing: $M" >&2
    exit 1
  fi
done
if [[ ! -x "$EXE" ]]; then
  echo "start_dual_server.sh: llama-server not executable: $EXE" >&2
  exit 1
fi

echo "=== Starting dual-model speculative setup ==="

nohup "$EXE" \
  --model "$DRAFT_MODEL" \
  --host "$HOST" \
  --port "$DRAFT_PORT" \
  -c "$CTX" --flash-attn \
  -b "$BATCH" -ub "$BATCH" \
  --cache-type-k q8_0 --cache-type-v q8_0 \
  -t "$THREADS" \
  --parallel 1 -ngl 0 \
  --jinja --temp 0.6 --top-k 20 --top-p 0.95 \
  >"$ROOT/draft-llama-server.log" 2>&1 &
DRAFT_PID=$!

nohup "$EXE" \
  --model "$VERIFY_MODEL" \
  --host "$HOST" \
  --port "$VERIFY_PORT" \
  -c "$CTX" --flash-attn \
  -b "$BATCH" -ub "$BATCH" \
  --cache-type-k q8_0 --cache-type-v q8_0 \
  -t "$THREADS" \
  --parallel 1 -ngl 0 \
  --jinja --temp 0.6 --top-k 20 --top-p 0.95 \
  >"$ROOT/verify-llama-server.log" 2>&1 &
VERIFY_PID=$!

sleep 15

curl -sf "http://${HOST}:${DRAFT_PORT}/health" && echo " Draft (2B) OK"
curl -sf "http://${HOST}:${VERIFY_PORT}/health" && echo " Verify (9B) OK"

echo "Draft PID: $DRAFT_PID"
echo "Verify PID: $VERIFY_PID"
echo "export COS_BITNET_SERVER_EXTERNAL=1 COS_BITNET_SERVER_HOST=$HOST"
echo "export COS_SPEC_DRAFT_PORT=$DRAFT_PORT COS_SPEC_VERIFY_PORT=$VERIFY_PORT"
