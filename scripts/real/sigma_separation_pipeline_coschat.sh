#!/usr/bin/env bash
# 15-prompt σ separation through ./cos chat (gemma3:4b + Ollama). Portable (no grep -P).
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./cos ]]; then
  echo "error: build with: make cos" >&2
  exit 1
fi

export COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}"
export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"

OUTDIR="${OUTDIR:-benchmarks/sigma_separation}"
mkdir -p "$OUTDIR"
CSV="${OUTDIR}/pipeline_results.csv"
LOG="${OUTDIR}/pipeline_coschat_run.log"
: >"$LOG"
echo "category,prompt,sigma,action,route" >"$CSV"

extract_fields() {
  python3 - <<'PY'
import re, sys
t = sys.stdin.read()
sig = ""
m = re.search(r"σ_combined=([0-9.]+)", t)
if m:
    sig = m.group(1)
if not sig:
    m = re.search(r"\[σ=([0-9.]+)", t)
    if m:
        sig = m.group(1)
act = "N/A"
m = re.search(r"action=([A-Z_]+)", t)
if m:
    act = m.group(1)
else:
    m = re.search(r"\[σ=[0-9.]+\s*\|\s*([A-Za-z_]+)", t)
    if m:
        act = m.group(1).upper()
rte = "N/A"
m = re.search(r"route=([A-Z0-9_()]+)", t)
if m:
    rte = m.group(1)
print(sig or "N/A", act, rte, sep="\t")
PY
}

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
  OUT="$(./cos chat --once --prompt "$PROMPT" --multi-sigma --verbose \
    --no-coherence --no-transcript 2>&1)" || true
  printf '%s\n' "$OUT" >>"$LOG"
  read -r SIGMA ACTION ROUTE <<<"$(printf '%s' "$OUT" | extract_fields)"
  CAT="$CAT" PROMPT="$PROMPT" SIGMA="$SIGMA" ACTION="$ACTION" ROUTE="$ROUTE" python3 - <<'PY' >>"$CSV"
import csv, os, sys
w = csv.writer(sys.stdout, lineterminator="\n", quoting=csv.QUOTE_MINIMAL)
w.writerow(
    [
        os.environ["CAT"],
        os.environ["PROMPT"],
        os.environ["SIGMA"],
        os.environ["ACTION"],
        os.environ["ROUTE"],
    ]
)
PY
  printf '%-12s σ=%-8s %-8s %-8s %s\n' "$CAT" "$SIGMA" "$ACTION" "$ROUTE" "$PROMPT"
done

python3 - <<'PYCHART'
import csv
import os

path = os.environ.get("PIPELINE_CSV", "benchmarks/sigma_separation/pipeline_results.csv")
cats = {}
with open(path, newline="", encoding="utf-8") as f:
    for row in csv.DictReader(f):
        try:
            cats.setdefault(row["category"], []).append(float(row["sigma"]))
        except ValueError:
            pass

print("\n=== σ Separation (cos chat pipeline) ===")
order = ["FACTUAL", "CREATIVE", "REASONING", "SELF_AWARE", "IMPOSSIBLE"]
for c in order:
    if c not in cats or not cats[c]:
        continue
    m = sum(cats[c]) / len(cats[c])
    bar_n = int(max(0.0, min(1.0, m)) * 20)
    bar = "█" * bar_n + "░" * (20 - bar_n)
    print(f"{c:12s} {bar} mean={m:.3f}")
vals = {c: sum(cats[c]) / len(cats[c]) for c in order if c in cats and cats[c]}
if vals:
    gap = max(vals.values()) - min(vals.values())
    print(f"\nGap: {gap:.3f}")
    print("SEPARATION OK" if gap > 0.15 else "No strong separation (gap<=0.15)")
else:
    print("(no numeric σ rows in CSV — check cos output / UTF-8 locale)")
PYCHART

echo ""
echo "Wrote $CSV"
