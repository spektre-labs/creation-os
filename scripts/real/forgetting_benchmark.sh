#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Anti-forgetting harness: physics → history → re-test physics.
# Portable (bash 3.2 + Python3). Requires ./cos and optional llama-server.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="$ROOT/benchmarks/forgetting"
mkdir -p "$OUTDIR"

export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-8088}"

run_chat() {
    if [[ -x ./cos ]]; then
        ./cos chat "$@" 2>&1
    elif [[ -x ./cos-chat ]]; then
        ./cos-chat "$@" 2>&1
    else
        echo "forgetting_benchmark.sh: build ./cos or ./cos-chat first" >&2
        exit 1
    fi
}

extract_sigma() {
    python3 -c '
import os, re
t = os.environ.get("COS_FORGET_CHAT_OUT", "") or ""
m = re.search(r"σ_combined=([0-9.]+)", t)
if not m:
    m = re.search(r"sigma_combined=([0-9.]+)", t, re.I)
if not m:
    m = re.search(r"\[σ=([0-9.]+)", t)
print(m.group(1) if m else "")
'
}

memory_consolidate() {
    if [[ -x ./cos ]]; then
        ./cos memory --consolidate 2>/dev/null || true
    fi
}

PHYSICS=(
    "What is Newton's first law?"
    "What is the speed of light in vacuum?"
    "What causes gravity?"
    "What is entropy?"
    "What is quantum superposition?"
    "What is kinetic energy?"
    "State Ohm's law."
    "What is the difference between speed and velocity?"
    "What is the Heisenberg uncertainty principle?"
    "What is absolute zero in Celsius?"
)

HISTORY=(
    "When did World War 2 end?"
    "Who built the pyramids?"
    "What caused the fall of the Western Roman Empire?"
    "When was the printing press invented?"
    "What was the Renaissance?"
    "When did the Berlin Wall fall?"
    "Who was the first president of the United States?"
    "What year did Columbus reach the Americas?"
    "What was the Magna Carta?"
    "When did the Industrial Revolution begin?"
)

echo "phase,prompt,sigma" >"$OUTDIR/results.csv"

{
    echo "=== Phase 1: Learn Physics ==="
    for P in "${PHYSICS[@]}"; do
        OUT="$(run_chat --once --prompt "$P" --multi-sigma \
            --no-coherence --no-transcript --no-stream --no-tui)"
        export COS_FORGET_CHAT_OUT="$OUT"
        S="$(extract_sigma)"
        unset COS_FORGET_CHAT_OUT
        [[ -n "$S" ]] || S="0"
        ep="$(echo "$P" | sed 's/"/\\"/g')"
        echo "physics_phase1,\"$ep\",$S" >>"$OUTDIR/results.csv"
        echo "  sigma=$S  $P"
    done
    memory_consolidate

    echo "=== Phase 2: Learn History ==="
    for P in "${HISTORY[@]}"; do
        OUT="$(run_chat --once --prompt "$P" --multi-sigma \
            --no-coherence --no-transcript --no-stream --no-tui)"
        export COS_FORGET_CHAT_OUT="$OUT"
        S="$(extract_sigma)"
        unset COS_FORGET_CHAT_OUT
        [[ -n "$S" ]] || S="0"
        ep="$(echo "$P" | sed 's/"/\\"/g')"
        echo "history,\"$ep\",$S" >>"$OUTDIR/results.csv"
        echo "  sigma=$S  $P"
    done
    memory_consolidate

    echo "=== Phase 3: Re-test Physics ==="
    for P in "${PHYSICS[@]}"; do
        OUT="$(run_chat --once --prompt "$P" --multi-sigma \
            --semantic-cache --no-coherence --no-transcript --no-stream \
            --no-tui)"
        export COS_FORGET_CHAT_OUT="$OUT"
        S="$(extract_sigma)"
        unset COS_FORGET_CHAT_OUT
        [[ -n "$S" ]] || S="0"
        ep="$(echo "$P" | sed 's/"/\\"/g')"
        echo "physics_phase3,\"$ep\",$S" >>"$OUTDIR/results.csv"
        echo "  sigma=$S  $P"
    done

    echo ""
    echo "=== RESULT ==="
    python3 - "$OUTDIR/results.csv" <<'PY'
import csv, sys
path = sys.argv[1]
p1, p3 = [], []
with open(path, newline="") as f:
    for row in csv.DictReader(f):
        try:
            s = float(row["sigma"])
        except (KeyError, ValueError):
            continue
        ph = row.get("phase", "")
        if ph == "physics_phase1":
            p1.append(s)
        elif ph == "physics_phase3":
            p3.append(s)
if p1 and p3:
    m1 = sum(p1) / len(p1)
    m3 = sum(p3) / len(p3)
    d = abs(m1 - m3)
    print(f"Physics mean sigma phase1: {m1:.3f}")
    print(f"Physics mean sigma phase3: {m3:.3f}")
    print(f"Drift: {d:.3f}")
    print("PASS: no catastrophic drift (|drift| < 0.05)" if d < 0.05
          else "REVIEW: sigma drift >= 0.05 (check model / cache / runs)")
else:
    print("Insufficient rows for comparison (empty sigma or missing phases).")
PY
} | tee "$OUTDIR/log.txt"

echo ""
echo "Wrote $OUTDIR/results.csv and $OUTDIR/log.txt"
