#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Start upstream llama.cpp llama-server with Qwen3-8B Q4_K_M weights.
# BitNet's bundled fork cannot load architecture "qwen3"; use Homebrew
#   brew install llama.cpp
# or set COS_BITNET_SERVER_EXE to any recent llama-server binary.
#
# Literal flags (matches install.sh / bitnet_server spawn intent):
#   --jinja --temp 0.6 --top-k 20 --top-p 0.95 -c <CTX> --parallel 1 -ngl 0
#
# Context window for llama-server (-c). Prefer COS_BITNET_CHAT_CTX (Creation OS
# default 2048 on 8GB machines), then COS_QWEN_SERVER_CTX, else 2048.
#
# Usage (repo root):
#   bash scripts/real/start_qwen_llama_server.sh
#   export COS_BITNET_SERVER_EXTERNAL=1 COS_BITNET_SERVER_PORT=8088
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

EXE="${COS_BITNET_SERVER_EXE:-/opt/homebrew/bin/llama-server}"
MODEL="${COS_BITNET_SERVER_MODEL:-$ROOT/models/qwen3-8b-Q4_K_M.gguf}"
HOST="${COS_BITNET_SERVER_HOST:-127.0.0.1}"
PORT="${COS_BITNET_SERVER_PORT:-8088}"
CTX="${COS_BITNET_CHAT_CTX:-${COS_QWEN_SERVER_CTX:-2048}}"
LOG="${COS_QWEN_SERVER_LOG:-$ROOT/qwen-llama-server.log}"

if [[ ! -x "$EXE" ]]; then
    echo "start_qwen_llama_server.sh: llama-server not executable: $EXE" >&2
    echo "  Install: brew install llama.cpp   (macOS) or build upstream llama.cpp." >&2
    exit 1
fi
if [[ ! -f "$MODEL" ]]; then
    echo "start_qwen_llama_server.sh: GGUF missing: $MODEL" >&2
    echo "  Run: bash scripts/real/setup_qwen_gguf.sh" >&2
    exit 1
fi

echo "start_qwen_llama_server.sh: $EXE (log: $LOG)"
nohup "$EXE" \
    --model "$MODEL" \
    --host "$HOST" \
    --port "$PORT" \
    -c "$CTX" \
    --parallel 1 \
    -ngl 0 \
    --jinja \
    --temp 0.6 \
    --top-k 20 \
    --top-p 0.95 \
    >"$LOG" 2>&1 &

echo "  pid=$!  → wait for /health then:"
echo "  export COS_BITNET_SERVER_EXTERNAL=1 COS_BITNET_SERVER_HOST=$HOST COS_BITNET_SERVER_PORT=$PORT"
