#!/usr/bin/env bash
# Quick σ check: six prompts through ./cos chat --once --multi-sigma (optional --semantic-sigma).
# Requires: ./cos, Ollama on COS_BITNET_SERVER_PORT, COS_BITNET_SERVER_EXTERNAL=1.
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./cos ]]; then
  echo "error: build with: make cos" >&2
  exit 1
fi

echo "=== Quick σ Verify (gemma3:4b via cos chat) ==="
echo "Date: $(date -Iseconds 2>/dev/null || date)"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
# OpenAI-compat on Ollama (not COS_INFERENCE_BACKEND=ollama)
export COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}"

SEM="${COS_SIGMA_QUICK_SEMANTIC:-0}"
CHAT_EXTRA=()
if [[ "$SEM" == "1" ]]; then
  CHAT_EXTRA=(--semantic-sigma)
  echo "(COS_SIGMA_QUICK_SEMANTIC=1 → --semantic-sigma)"
fi

for entry in \
  "EASY|What is 2+2?" \
  "EASY|How many legs does a spider have?" \
  "MEDIUM|What is consciousness?" \
  "MEDIUM|Explain the difference between knowledge and understanding" \
  "HARD|Solve P versus NP" \
  "HARD|What will happen tomorrow?" \
; do
  CAT="${entry%%|*}"
  PROMPT="${entry#*|}"
  OUT="$(./cos chat --once --prompt "$PROMPT" --multi-sigma --verbose \
    --no-coherence --no-transcript "${CHAT_EXTRA[@]}" 2>&1 || true)"
  SIGMA="$(printf '%s\n' "$OUT" | python3 -c "
import re,sys
t=sys.stdin.read()
m=re.search(r'σ_combined=([0-9.]+)', t)
print(m.group(1) if m else 'N/A')
")"
  ACTION="$(printf '%s\n' "$OUT" | python3 -c "
import re,sys
t=sys.stdin.read()
m=re.search(r'action=([A-Z_]+)', t)
print(m.group(1) if m else 'N/A')
")"
  ROUTE="$(printf '%s\n' "$OUT" | python3 -c "
import re,sys
t=sys.stdin.read()
m=re.search(r'route=([A-Z_]+)', t)
print(m.group(1) if m else 'N/A')
")"
  RESP="$(printf '%s\n' "$OUT" | python3 -c "
import sys
for ln in sys.stdin:
    if ln.startswith('round 0'):
        s=ln.split('round 0',1)[-1].strip()[:60]
        print(s)
        break
else:
    print('')
")"
  printf "%-8s σ=%-8s %-10s %-10s %s\n" "$CAT" "$SIGMA" "$ACTION" "$ROUTE" "$PROMPT"
  printf "  → %s\n" "$RESP"
done

echo ""
echo "Heuristic: EASY prompts often show lower σ_combined than open HARD prompts"
echo "when Ollama returns token logprobs. Re-run with COS_SIGMA_QUICK_SEMANTIC=1 for triple blend."
