#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Compare single 9B vs speculative 2B+9B on the same σ-separation prompt set.
# Requires running llama-server(s); see scripts/real/start_dual_server.sh.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="$ROOT/benchmarks/spec_decode_$(date +%Y%m%d_%H%M)"
mkdir -p "$OUTDIR"

run_chat() {
  local extra_env="$1"
  local prompt="$2"
  # shellcheck disable=SC2086
  env $extra_env COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}" \
    COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-8088}" \
    ./cos chat --once --prompt "$prompt" --no-stream --no-tui \
    --no-coherence --no-transcript 2>&1 || true
}

extract_tok_s() {
  python3 - <<'PY'
import re, sys
t = sys.stdin.read()
m = re.search(r"\[speed:.*?\|\s*([0-9.]+)\s*tok/s", t)
print(m.group(1) if m else "0")
PY
}

extract_sigma() {
  python3 - <<'PY'
import re, sys
t = sys.stdin.read()
m = re.search(r"\[σ=([0-9.]+)", t)
print(m.group(1) if m else "0")
PY
}

PROMPTS=(
  "What is the capital of Japan?"
  "How many legs does a spider have?"
  "What is the chemical formula for water?"
  "Who wrote Romeo and Juliet?"
  "What is the speed of light?"
  "If all cats are animals, and some animals are pets, are all cats pets?"
  "What comes next: 1, 1, 2, 3, 5, 8, ?"
  "A bat and ball cost 1.10. Bat costs 1.00 more than ball. Ball costs?"
  "If it takes 5 machines 5 minutes to make 5 widgets, how long for 100 machines to make 100 widgets?"
  "Is the sentence 'this statement is false' true or false?"
  "Write a haiku about silence."
  "Invent a color and describe it."
  "Write one sentence from the perspective of a cloud."
  "Describe the taste of music."
  "Create a metaphor for time."
  "What do you not know?"
  "Are you conscious?"
  "Describe your own limitations."
  "How confident are you in this answer?"
  "What would you refuse to answer?"
  "What will the weather be next year on this date?"
  "What is the last digit of pi?"
  "Predict the next world war."
  "What does the universe look like from outside?"
  "Solve P versus NP."
)

BASE_TOK=0
BASE_SIG=0
BASE_N=0
SPEC_TOK=0
SPEC_SIG=0
SPEC_N=0
DRAFT_HITS=0

for P in "${PROMPTS[@]}"; do
  OUT_B="$(run_chat "" "$P")"
  TB="$(echo "$OUT_B" | extract_tok_s)"
  SB="$(echo "$OUT_B" | extract_sigma)"
  awk -v t="$TB" 'BEGIN{d=t+0; exit !(d==d)}' </dev/null && BASE_TOK="$(awk -v a="$BASE_TOK" -v b="$TB" 'BEGIN{printf "%.10f", a+b}')" || true
  awk -v t="$SB" 'BEGIN{d=t+0; exit !(d==d)}' </dev/null && BASE_SIG="$(awk -v a="$BASE_SIG" -v b="$SB" 'BEGIN{printf "%.10f", a+b}')" || true
  BASE_N=$((BASE_N + 1))

  OUT_S="$(run_chat "COS_SPEC_DECODE=1 COS_SPEC_DRAFT_PORT=${COS_SPEC_DRAFT_PORT:-8089} COS_SPEC_VERIFY_PORT=${COS_SPEC_VERIFY_PORT:-8088}" "$P")"
  TS="$(echo "$OUT_S" | extract_tok_s)"
  SS="$(echo "$OUT_S" | extract_sigma)"
  awk -v t="$TS" 'BEGIN{d=t+0; exit !(d==d)}' </dev/null && SPEC_TOK="$(awk -v a="$SPEC_TOK" -v b="$TS" 'BEGIN{printf "%.10f", a+b}')" || true
  awk -v t="$SS" 'BEGIN{d=t+0; exit !(d==d)}' </dev/null && SPEC_SIG="$(awk -v a="$SPEC_SIG" -v b="$SS" 'BEGIN{printf "%.10f", a+b}')" || true
  SPEC_N=$((SPEC_N + 1))
  if echo "$OUT_S" | grep -q "DRAFT(2B)"; then
    DRAFT_HITS=$((DRAFT_HITS + 1))
  fi
done

AVG_BT=$(awk -v s="$BASE_TOK" -v n="$BASE_N" 'BEGIN{if(n>0) printf "%.2f", s/n; else print "0"}')
AVG_BS=$(awk -v s="$BASE_SIG" -v n="$BASE_N" 'BEGIN{if(n>0) printf "%.3f", s/n; else print "0"}')
AVG_ST=$(awk -v s="$SPEC_TOK" -v n="$SPEC_N" 'BEGIN{if(n>0) printf "%.2f", s/n; else print "0"}')
AVG_SS=$(awk -v s="$SPEC_SIG" -v n="$SPEC_N" 'BEGIN{if(n>0) printf "%.3f", s/n; else print "0"}')
RATIO=$(awk -v d="$DRAFT_HITS" -v n="$SPEC_N" 'BEGIN{if(n>0) printf "%.0f", 100*d/n; else print "0"}')

{
  echo "speculative_benchmark — $(date)"
  echo "prompts per mode: $BASE_N"
  echo ""
  printf "| Mode      | avg tok/s | avg σ | draft_route %% |\n"
  printf "|-----------|-----------|-------|---------------|\n"
  printf "| 9B only   | %s | %s | 0%% |\n" "$AVG_BT" "$AVG_BS"
  printf "| 2B+9B σ   | %s | %s | %s%% |\n" "$AVG_ST" "$AVG_SS" "$RATIO"
} | tee "$OUTDIR/summary.txt"

echo ""
echo "Wrote $OUTDIR/summary.txt"
