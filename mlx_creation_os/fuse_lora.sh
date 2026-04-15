#!/usr/bin/env bash
# Fuse LoRA adapter into base weights → creation_os_3b_fused (Phase 3)
set -euo pipefail
cd "$(dirname "$0")"
ADAPTER="${1:-creation_os_adapter}"
MODEL="${2:-mlx-community/Llama-3.2-3B-Instruct-4bit}"
OUT="${3:-creation_os_3b_fused}"

python3 -m mlx_lm fuse \
  --model "$MODEL" \
  --adapter-path "$ADAPTER" \
  --save-path "$OUT"

echo "Fused model at: $OUT"
