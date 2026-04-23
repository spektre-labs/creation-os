#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# STEP 1 (Qwen3-8B): matches the documented Hub command first; renames the
# result to models/qwen3-8b-Q4_K_M.gguf.  The canonical Hub blob is often
# named Qwen3-8B-Q4_K_M.gguf — if the literal name is absent, that file is
# fetched instead.
#
# Literal one-liner (from repo root):
#   huggingface-cli download Qwen/Qwen3-8B-GGUF qwen3-8b-Q4_K_M.gguf --local-dir models/
#
# Optional (larger disk, Qwen3.5-9B): override repo + file + local name, e.g.:
#   COS_QWEN_HF_REPO=unsloth/Qwen3.5-9B-GGUF \
#   COS_QWEN_HF_FILE=Qwen3.5-9B-Q4_K_M.gguf \
#   COS_QWEN_LOCAL_NAME=qwen3.5-9b-Q4_K_M.gguf \
#     bash scripts/real/setup_qwen_gguf.sh
#
# Usage (from repo root):
#   bash scripts/real/setup_qwen_gguf.sh
#
# Prerequisites: huggingface-cli or hf (pip install huggingface_hub) on PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
MODELS="$ROOT/models"
DEST="${COS_QWEN_LOCAL_DEST:-$MODELS/qwen3-8b-Q4_K_M.gguf}"

hf_download() {
    if command -v hf >/dev/null 2>&1; then
        hf download "$@"
    elif command -v huggingface-cli >/dev/null 2>&1; then
        huggingface-cli download "$@"
    else
        echo "setup_qwen_gguf.sh: need hf or huggingface-cli on PATH" >&2
        exit 1
    fi
}

mkdir -p "$MODELS"

if [[ -f "$DEST" ]]; then
    echo "setup_qwen_gguf.sh: already present: $DEST"
    exit 0
fi

REPO="${COS_QWEN_HF_REPO:-Qwen/Qwen3-8B-GGUF}"
REMOTE_OVERRIDE="${COS_QWEN_HF_FILE:-}"

if [[ -n "$REMOTE_OVERRIDE" ]]; then
    echo "setup_qwen_gguf.sh: downloading $REPO $REMOTE_OVERRIDE → $MODELS"
    hf_download "$REPO" "$REMOTE_OVERRIDE" --local-dir "$MODELS"
    REMOTE_FILE="$REMOTE_OVERRIDE"
else
    echo "setup_qwen_gguf.sh: STEP 1 literal: huggingface-cli download $REPO qwen3-8b-Q4_K_M.gguf --local-dir models/"
    set +e
    hf_download "$REPO" qwen3-8b-Q4_K_M.gguf --local-dir "$MODELS"
    rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        echo "setup_qwen_gguf.sh: literal name missing on Hub; fetching Qwen3-8B-Q4_K_M.gguf"
        hf_download "$REPO" Qwen3-8B-Q4_K_M.gguf --local-dir "$MODELS"
        REMOTE_FILE="Qwen3-8B-Q4_K_M.gguf"
    else
        REMOTE_FILE="qwen3-8b-Q4_K_M.gguf"
    fi
fi

CAND="$MODELS/$REMOTE_FILE"
if [[ ! -f "$CAND" ]]; then
    found="$(find "$MODELS" -name "$REMOTE_FILE" -type f 2>/dev/null | head -1 || true)"
    if [[ -n "$found" ]]; then
        CAND="$found"
    else
        echo "setup_qwen_gguf.sh: could not locate downloaded $REMOTE_FILE under $MODELS" >&2
        exit 1
    fi
fi

if [[ "$CAND" != "$DEST" ]]; then
    mv "$CAND" "$DEST"
fi

echo "setup_qwen_gguf.sh: ready: $DEST"
echo ""
echo "  export COS_BITNET_SERVER_MODEL=\"$DEST\""
