#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Fetch Qwen3.5-2B Q4_K_M GGUF (~1.5 GB) for COS_MODEL_SIZE=small / fast decomposition.
# Canonical filename: models/qwen3.5-2b-Q4_K_M.gguf
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

MODEL_DIR="${COS_MODEL_DIR:-models}"
CANON="qwen3.5-2b-Q4_K_M.gguf"
HF_REPO="${COS_QWEN35_2B_REPO:-unsloth/Qwen3.5-2B-GGUF}"
HF_FILE="${COS_QWEN35_2B_FILE:-Qwen3.5-2B-Q4_K_M.gguf}"

mkdir -p "$MODEL_DIR"

if [[ -f "$MODEL_DIR/$CANON" ]]; then
    echo "Model already exists: $MODEL_DIR/$CANON"
    exit 0
fi

echo "Downloading Qwen3.5-2B Q4_K_M ($HF_REPO) ..."

if command -v huggingface-cli &>/dev/null; then
    huggingface-cli download "$HF_REPO" "$HF_FILE" --local-dir "$MODEL_DIR"
    if [[ -f "$MODEL_DIR/$HF_FILE" && "$HF_FILE" != "$CANON" ]]; then
        mv -f "$MODEL_DIR/$HF_FILE" "$MODEL_DIR/$CANON"
    fi
else
    echo "Install huggingface-cli: pip install huggingface_hub" >&2
    echo "Or download manually from https://huggingface.co/$HF_REPO" >&2
    exit 1
fi

echo "Done: $MODEL_DIR/$CANON"
ls -lh "$MODEL_DIR/$CANON"
