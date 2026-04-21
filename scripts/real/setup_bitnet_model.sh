#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Download the Microsoft BitNet-b1.58 2B GGUF bundle and run bitnet.cpp
# setup_env.py for the i2_s quant.  Intended for maintainer / power-user
# machines with network access (not run by default from install.sh).
#
# Usage (from repo root):
#   bash scripts/real/setup_bitnet_model.sh
#
# Prerequisites: python3, cmake, a C++ compiler, huggingface-cli, git
# submodules initialized (third_party/bitnet).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BITNET="$ROOT/third_party/bitnet"
MODEL_DIR="$ROOT/models/BitNet-b1.58-2B-4T"
GGUF="$MODEL_DIR/ggml-model-i2_s.gguf"

need() {
    command -v "$1" >/dev/null 2>&1 \
        || { echo "setup_bitnet_model.sh: missing: $1" >&2; exit 1; }
}

need python3
need cmake
need huggingface-cli
if [[ ! -f "$BITNET/setup_env.py" ]]; then
    echo "setup_bitnet_model.sh: expected $BITNET/setup_env.py (init submodules?)" >&2
    exit 1
fi

mkdir -p "$MODEL_DIR"

if [[ ! -f "$GGUF" ]]; then
    echo "Downloading microsoft/BitNet-b1.58-2B-4T-gguf → $MODEL_DIR"
    huggingface-cli download microsoft/BitNet-b1.58-2B-4T-gguf \
        --local-dir "$MODEL_DIR"
fi

cd "$BITNET"
if ! pip install -r requirements.txt >/dev/null 2>&1; then
    pip install -r requirements.txt --break-system-packages \
        || pip install -r requirements.txt
fi

echo "Running setup_env.py (-q i2_s)…"
python3 setup_env.py -md "$MODEL_DIR" -q i2_s

echo ""
echo "Next (example):"
echo "  export CREATION_OS_BITNET_EXE=$BITNET/build/bin/llama-cli"
echo "  export CREATION_OS_BITNET_MODEL=$GGUF"
echo "  ./cos chat --once --prompt \"What is 2+2?\""
