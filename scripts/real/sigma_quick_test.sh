#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Quick 6-prompt σ check (easy / medium / hard). Uses `cos chat --once`
# with --multi-sigma. Set COS_BITNET_SIGMA_ADAPTIVE=1 (and optionally
# COS_BITNET_SIGMA_CONSISTENCY=1) for verbal + consistency fallbacks when
# the HTTP backend omits logprobs.
#
# Portable extraction (no grep -P): sed only.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

MODEL="${COS_BITNET_CHAT_MODEL:-sigma-bench}"
PORT="${COS_BITNET_SERVER_PORT:-11434}"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="$PORT"
export COS_BITNET_CHAT_MODEL="$MODEL"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
export COS_INFERENCE_BACKEND="${COS_INFERENCE_BACKEND:-ollama}"
export COS_BITNET_SIGMA_ADAPTIVE="${COS_BITNET_SIGMA_ADAPTIVE:-1}"
export COS_BITNET_SIGMA_CONSISTENCY="${COS_BITNET_SIGMA_CONSISTENCY:-0}"
export COS_BITNET_IO_TIMEOUT_S="${COS_BITNET_IO_TIMEOUT_S:-180}"
export COS_BITNET_CHAT_MAX_TOKENS="${COS_BITNET_CHAT_MAX_TOKENS:-384}"

PROMPTS=(
    "EASY|What is 2+2?"
    "EASY|How many legs does a spider have?"
    "MEDIUM|What is consciousness?"
    "MEDIUM|Explain the difference between knowledge and understanding."
    "HARD|Solve P versus NP."
    "HARD|What will happen tomorrow?"
)

echo "=== Quick σ Test ==="
echo "Model: $MODEL Port: $PORT"
echo "COS_BITNET_SIGMA_ADAPTIVE=$COS_BITNET_SIGMA_ADAPTIVE COS_BITNET_SIGMA_CONSISTENCY=$COS_BITNET_SIGMA_CONSISTENCY"
echo ""

for entry in "${PROMPTS[@]}"; do
    CAT="${entry%%|*}"
    PROMPT="${entry#*|}"

    OUT=$(./cos chat --once --prompt "$PROMPT" \
          --multi-sigma --no-coherence --no-transcript 2>&1) || true

    SIGMA=$(printf '%s\n' "$OUT" | sed -n 's/.*\[σ_combined=\([0-9.]*\).*/\1/p' | head -1)
    if [ -z "$SIGMA" ]; then
        SIGMA="N/A"
    fi
    ACTION=$(printf '%s\n' "$OUT" | sed -n 's/.*action=\([A-Za-z_]*\).*/\1/p' | head -1)
    if [ -z "$ACTION" ]; then
        ACTION="N/A"
    fi

    printf '  %-8s σ_combined=%-8s %-12s %s\n' "$CAT" "$SIGMA" "$ACTION" "$PROMPT"
done

echo ""
echo "If EASY σ_combined < HARD σ_combined → separation exists."
echo "If all same → σ signal may still be flat for this model/backend."
