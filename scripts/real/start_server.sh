#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Start optimized llama-server for Qwen3.5 (or fallback Qwen3-8B).
# COS_MODEL_SIZE: small → 2B + larger default ctx; medium → 9B (8 GB friendly).
#
# Usage (repo root):
#   COS_MODEL_SIZE=medium bash scripts/real/start_server.sh
#   export COS_BITNET_SERVER_EXTERNAL=1 COS_BITNET_SERVER_PORT=8088
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

EXE="${COS_BITNET_SERVER_EXE:-/opt/homebrew/bin/llama-server}"
HOST="${COS_BITNET_SERVER_HOST:-127.0.0.1}"
SIZE="${COS_MODEL_SIZE:-medium}"
PORT="${COS_LLAMA_PORT:-${COS_BITNET_SERVER_PORT:-8088}}"
THREADS="${COS_LLAMA_THREADS:-4}"
BATCH="${COS_LLAMA_BATCH:-512}"

CTX="${COS_LLAMA_CTX:-}"
MODEL=""

case "$SIZE" in
    small)
        MODEL="${COS_MODEL_SMALL_PATH:-$ROOT/models/qwen3.5-2b-Q4_K_M.gguf}"
        if [[ -z "${CTX:-}" ]]; then
            CTX="${COS_LLAMA_CTX_SMALL:-4096}"
        fi
        ;;
    medium)
        MODEL="${COS_MODEL_PATH:-$ROOT/models/qwen3.5-9b-Q4_K_M.gguf}"
        if [[ -z "${CTX:-}" ]]; then
            CTX="${COS_LLAMA_CTX:-2048}"
        fi
        ;;
    *)
        MODEL="${COS_MODEL_PATH:-$ROOT/models/qwen3-8b-Q4_K_M.gguf}"
        if [[ -z "${CTX:-}" ]]; then
            CTX="${COS_LLAMA_CTX:-2048}"
        fi
        ;;
esac

if [[ ! -f "$MODEL" ]]; then
    FB="$ROOT/models/qwen3-8b-Q4_K_M.gguf"
    if [[ -f "$FB" ]]; then
        echo "start_server.sh: using fallback weights: $FB"
        MODEL="$FB"
    else
        echo "start_server.sh: model not found: $MODEL" >&2
        echo "  Run: bash scripts/real/download_qwen35_9b.sh" >&2
        echo "  Or:  bash scripts/real/download_qwen35_2b.sh  (small)" >&2
        exit 1
    fi
fi

if [[ ! -x "$EXE" ]]; then
    echo "start_server.sh: llama-server not executable: $EXE" >&2
    exit 1
fi

echo "Starting server: $MODEL (ctx=$CTX, threads=$THREADS, port=$PORT)"

"$EXE" \
    --model "$MODEL" \
    --host "$HOST" \
    --port "$PORT" \
    -c "$CTX" \
    --flash-attn \
    -b "$BATCH" -ub "$BATCH" \
    --cache-type-k q8_0 --cache-type-v q8_0 \
    -t "$THREADS" \
    --mlock \
    --parallel 1 \
    -ngl 0 \
    --jinja \
    --temp 0.6 \
    --top-k 20 \
    --top-p 0.95 \
    >/tmp/cos-start-server.log 2>&1 &

sleep 10
if curl -sf "http://127.0.0.1:${PORT}/health" >/dev/null; then
    echo " Server OK (http://127.0.0.1:${PORT}/health)"
else
    echo " Server FAILED — see /tmp/cos-start-server.log" >&2
    exit 1
fi
