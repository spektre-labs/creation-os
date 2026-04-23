#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Measure cos chat throughput for six fixed prompts. Requires a running
# inference backend (llama-server or Ollama). Writes CSV under benchmarks/speed/.
#
# Usage (repo root):
#   bash scripts/real/speed_benchmark.sh
#   COS_INFERENCE_BACKEND=ollama bash scripts/real/speed_benchmark.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUT_DIR="$ROOT/benchmarks/speed"
CSV="$OUT_DIR/speed_results.csv"
mkdir -p "$OUT_DIR"

BACKEND="${COS_INFERENCE_BACKEND:-llama-server}"
TS="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

if [[ ! -x ./cos-chat ]] && [[ ! -x ./cos ]]; then
    echo "speed_benchmark.sh: build cos-chat first: make cos-chat (or make cos)" >&2
    exit 1
fi

run_chat() {
    local pr="$1"
    if [[ -x ./cos-chat ]]; then
        ./cos-chat --once --prompt "$pr" --max-tokens 256 2>&1
    else
        ./cos chat --once --prompt "$pr" --max-tokens 256 2>&1
    fi
}

PROMPTS=(
    "What is 2+2?"
    "Name three colors."
    "Explain photosynthesis in two sentences."
    "Write a short paragraph about Helsinki."
    "What are the differences between Python and C?"
    "Explain relativity to a teenager."
)

{
    echo "# speed_benchmark backend=$BACKEND ts=$TS"
    echo "idx,prompt,backend,tokens,wall_ms,tok_per_s,sigma"
} >"$CSV"

sum_tps=0
sum_lat=0
sum_sig=0
min_tps=""
max_tps=""
n_run=0

for i in "${!PROMPTS[@]}"; do
    p="${PROMPTS[$i]}"
    OUT="$(run_chat "$p" || true)"
    speed_line="$(echo "$OUT" | grep '^\[speed:' | tail -n1 || true)"
    receipt="$(echo "$OUT" | grep '^\[σ=' | tail -n1 || true)"
    tok="0"
    wall="0"
    tps="0"
    if [[ -n "$speed_line" ]]; then
        tok="$(echo "$speed_line" | sed -n 's/.*\[speed: *\([0-9]*\) tok.*/\1/p')"
        wall="$(echo "$speed_line" | sed -n 's/.*| *\([0-9]*\)ms.*/\1/p')"
        tps="$(echo "$speed_line" | sed -n 's/.*| *\([0-9.]*\) tok\/s\].*/\1/p')"
    fi
    sig="$(echo "$receipt" | sed -n 's/^\[σ=\([0-9.]*\).*/\1/p')"
    [[ -n "$sig" ]] || sig="0"
    idx=$((i + 1))
    ep="$(echo "$p" | sed 's/"/\\"/g')"
    echo "$idx,\"$ep\",$BACKEND,$tok,$wall,$tps,$sig" >>"$CSV"

    if awk -v t="$tps" 'BEGIN {exit !(t + 0 > 0)}' </dev/null; then
        sum_tps="$(awk -v a="$sum_tps" -v b="$tps" 'BEGIN {printf "%.10f", a+b}')"
        sum_lat="$(awk -v a="$sum_lat" -v b="$wall" 'BEGIN {printf "%.10f", a+b}')"
        sum_sig="$(awk -v a="$sum_sig" -v b="$sig" 'BEGIN {printf "%.10f", a+b}')"
        if [[ -z "$min_tps" ]] || awk -v m="$min_tps" -v x="$tps" 'BEGIN {exit !(x<m)}'; then
            min_tps="$tps"
        fi
        if [[ -z "$max_tps" ]] || awk -v m="$max_tps" -v x="$tps" 'BEGIN {exit !(x>m)}'; then
            max_tps="$tps"
        fi
        n_run=$((n_run + 1))
    fi
done

echo ""
echo "=== Summary ($BACKEND) ==="
if [[ "$n_run" -gt 0 ]]; then
    avg_tps="$(awk -v s="$sum_tps" -v n="$n_run" 'BEGIN {printf "%.2f", s/n}')"
    avg_lat="$(awk -v s="$sum_lat" -v n="$n_run" 'BEGIN {printf "%.1f", s/n}')"
    avg_sig="$(awk -v s="$sum_sig" -v n="$n_run" 'BEGIN {printf "%.4f", s/n}')"
    echo "  runs with tok/s > 0: $n_run"
    echo "  avg tok/s: $avg_tps  (min $min_tps  max $max_tps)"
    echo "  avg latency ms: $avg_lat"
    echo "  avg σ: $avg_sig"
else
    echo "  (no speed lines parsed — is the inference server running?)"
fi
echo "  CSV: $CSV"
