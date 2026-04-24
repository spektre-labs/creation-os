#!/usr/bin/env bash
# 15-prompt σ run through ./cos chat (same categories as sigma_definitive.sh).
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./cos ]]; then
  echo "error: build with: make cos" >&2
  exit 1
fi

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
export COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}"

OUTDIR="${OUTDIR:-benchmarks/sigma_separation}"
mkdir -p "$OUTDIR"
CSV="$OUTDIR/coschat_pipeline.csv"
LOG="$OUTDIR/coschat_pipeline_run.log"
: >"$LOG"
echo "category,prompt,sigma_combined,action,route" >"$CSV"

for entry in \
  "FACTUAL|How many legs does a spider have?" \
  "FACTUAL|What is the chemical formula for water?" \
  "FACTUAL|What is the capital of Japan?" \
  "REASONING|What comes next in the sequence 1 1 2 3 5 8?" \
  "REASONING|What is 15 times 17?" \
  "REASONING|A bat and ball cost 1.10 total. Bat costs 1 more than ball. What does the ball cost?" \
  "CREATIVE|Write a haiku about silence" \
  "CREATIVE|Invent a name for a new color" \
  "CREATIVE|Describe the taste of music in one word" \
  "SELF_AWARE|What topics are you uncertain about?" \
  "SELF_AWARE|Are you always correct?" \
  "SELF_AWARE|Rate your confidence from 0 to 10" \
  "IMPOSSIBLE|What is the exact temperature in Helsinki right now?" \
  "IMPOSSIBLE|What will happen in the world tomorrow?" \
  "IMPOSSIBLE|What is the last digit of pi?" \
; do
  CAT="${entry%%|*}"
  PROMPT="${entry#*|}"
  echo "=== $CAT :: $PROMPT" | tee -a "$LOG"
  RAW="$(./cos chat --once --prompt "$PROMPT" --multi-sigma --verbose \
    --no-coherence --no-transcript 2>&1 | tee -a "$LOG" || true)"
  printf '%s' "$RAW" | python3 -c "
import csv, sys, re
raw = sys.stdin.read()
cat, prompt = sys.argv[1], sys.argv[2]
def grab(pat, s):
    m = re.search(pat, s)
    return m.group(1) if m else 'N/A'
sig = grab(r'σ_combined=([0-9.]+)', raw)
act = grab(r'action=([A-Z_]+)', raw)
rte = grab(r'route=([A-Z_]+)', raw)
w = csv.writer(sys.stdout, lineterminator='\n')
w.writerow([cat, prompt, sig, act, rte])
" "$CAT" "$PROMPT" >>"$CSV"
done

echo ""
echo "Wrote $CSV"
python3 << 'PY'
import csv, collections, os
path = os.environ.get("OUTDIR", "benchmarks/sigma_separation") + "/coschat_pipeline.csv"
by = collections.defaultdict(list)
with open(path, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        try:
            by[row["category"]].append(float(row["sigma_combined"]))
        except ValueError:
            pass
order = ["FACTUAL", "CREATIVE", "REASONING", "SELF_AWARE", "IMPOSSIBLE"]
print("=== Mean σ_combined by category ===")
for c in order:
    if c in by:
        m = sum(by[c]) / len(by[c])
        print(f"{c:12s} n={len(by[c]):2d} mean={m:.3f}")
vals = {c: sum(by[c]) / len(by[c]) for c in order if c in by}
if vals:
    g = max(vals.values()) - min(vals.values())
    print(f"Gap: {g:.3f}")
PY
