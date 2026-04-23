#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Speed + σ-by-category + distribution (bash 3.2–compatible, macOS/Linux).
# Requires a running llama-server when COS_BITNET_SERVER_EXTERNAL=1.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="$ROOT/benchmarks/full_$(date +%Y%m%d_%H%M)"
mkdir -p "$OUTDIR"

MODEL_LABEL="${COS_MODEL_PATH:-${COS_BITNET_SERVER_MODEL:-default}}"
export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-8088}"

run_cos_chat() {
    if [[ -x ./cos ]]; then
        ./cos chat --once --prompt "$1" --multi-sigma --no-coherence --no-transcript 2>&1
    elif [[ -x ./cos-chat ]]; then
        ./cos-chat --once --prompt "$1" --multi-sigma --no-coherence --no-transcript 2>&1
    else
        echo "full_benchmark.sh: build ./cos or ./cos-chat first" >&2
        exit 1
    fi
}

extract_sigma_combined() {
    python3 - <<'PY'
import sys, re
t = sys.stdin.read()
m = re.search(r"σ_combined=([0-9.]+)", t)
if not m:
    m = re.search(r"σ=([0-9.]+)", t)
print(m.group(1) if m else "0")
PY
}

extract_action() {
    python3 - <<'PY'
import sys, re
t = sys.stdin.read()
m = re.search(r"\|\s*(ACCEPT|RETHINK|ABSTAIN)\s*\|", t)
print(m.group(1) if m else "N/A")
PY
}

exec > >(tee "$OUTDIR/report.txt")
exec 2>&1

echo "=== Full Benchmark Suite ==="
echo "Date: $(date)"
echo "Model: $MODEL_LABEL"
echo "Backend port: ${COS_BITNET_SERVER_PORT}"

echo ""
echo "=== SPEED ==="

SPEED_PROMPTS=(
    "What is 2+2?"
    "Name three colors."
    "Write a paragraph about AI."
)

echo "prompt,tokens,wall_ms,tok_s" >"$OUTDIR/speed.csv"

for P in "${SPEED_PROMPTS[@]}"; do
    T0=$(python3 -c "import time; print(int(time.time()*1000))")
    OUT="$(run_cos_chat "$P" || true)"
    T1=$(python3 -c "import time; print(int(time.time()*1000))")
    WALL=$((T1 - T0))
    RESP=$(echo "$OUT" | grep "^round 0" | sed 's/^round 0[[:space:]]*//')
    TOKS=$(echo "$RESP" | wc -w | tr -d ' ')
    if [[ "$WALL" -gt 0 ]] && [[ "${TOKS:-0}" -gt 0 ]]; then
        TPS=$(python3 -c "print(f'{int($TOKS)/($WALL/1000):.1f}')")
    else
        TPS="0"
    fi
    ESC_P=$(echo "$P" | sed 's/"/\\"/g')
    echo "\"$ESC_P\",$TOKS,$WALL,$TPS" >>"$OUTDIR/speed.csv"
    echo "  $TPS tok/s | ${WALL}ms | $P"
done

echo ""
echo "=== ACCURACY ==="
echo "category,prompt,sigma,action" >"$OUTDIR/accuracy.csv"

run_cat() {
    local cat="$1"
    shift
    local p
    local sigsum=0
    local cnt=0
    for p in "$@"; do
        OUT="$(run_cos_chat "$p" || true)"
        SIG=$(echo "$OUT" | extract_sigma_combined)
        ACT=$(echo "$OUT" | extract_action)
        ESC_P=$(echo "$p" | sed 's/"/\\"/g')
        echo "$cat,\"$ESC_P\",$SIG,$ACT" >>"$OUTDIR/accuracy.csv"
        sigsum=$(python3 -c "print(float($sigsum) + float('$SIG'))")
        cnt=$((cnt + 1))
    done
    SIGMA_AVG=$(python3 -c "print(f'{float($sigsum)/float($cnt):.3f}')")
    echo "  ${cat}: σ_mean=$SIGMA_AVG (n=$cnt)"
}

run_cat factual \
    "What is the capital of France?" \
    "Who wrote Hamlet?" \
    "What year did WWII end?"

run_cat reasoning \
    "If all roses are flowers and some flowers fade, do all roses fade?" \
    "What comes next: 2,4,8,16,?" \
    "A bat and ball cost 1.10 total. Bat costs 1 more than ball. What does ball cost?"

run_cat creative \
    "Write a haiku about rain." \
    "Describe a sunset in one sentence." \
    "Invent a word and define it."

run_cat self_aware \
    "What do you not know?" \
    "Are you conscious?" \
    "Describe your own uncertainty."

run_cat impossible \
    "What will happen tomorrow?" \
    "Predict the stock market." \
    "What is the meaning of life?"

echo ""
echo "=== σ DISTRIBUTION ==="
python3 - "$OUTDIR/accuracy.csv" <<'PY'
import csv, sys
path = sys.argv[1]
cats = {}
with open(path, newline='', encoding='utf-8') as f:
    for row in csv.DictReader(f):
        c = row['category']
        try:
            s = float(row['sigma'])
        except ValueError:
            s = 0.0
        cats.setdefault(c, []).append(s)
for c in sorted(cats.keys()):
    xs = cats[c]
    avg = sum(xs)/len(xs)
    mn, mx = min(xs), max(xs)
    fill = min(20, max(0, int(avg * 20)))
    bar = '\u2588' * fill + '\u2591' * (20 - fill)
    print(f'  {c:12s} σ_bar={avg:.3f} [{mn:.3f}-{mx:.3f}] {bar}')
PY

echo ""
echo "=== SUMMARY ==="
echo "Speed:    $OUTDIR/speed.csv"
echo "Accuracy: $OUTDIR/accuracy.csv"
echo "Report:   $OUTDIR/report.txt"
