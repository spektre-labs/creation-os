#!/usr/bin/env bash
# Standalone Ω evolution: 5 runs × 10 prompts, σ_mean per run (portable; no grep -P).
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Env:
#   OUTDIR              default benchmarks/evolve_campaign
#   COS_EVOLVE_FINAL_CLEAN=1  rm ~/.cos/engram_*.db before run (destructive)
#   CHAT_TIMEOUT_S      default 300
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./cos ]]; then
  echo "error: ./cos missing — make cos" >&2
  exit 1
fi

export COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}"
export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}"
export COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-0}"

OUTDIR="${OUTDIR:-benchmarks/evolve_campaign}"
mkdir -p "$OUTDIR"

if [[ "${COS_EVOLVE_FINAL_CLEAN:-0}" == "1" ]]; then
  rm -f "${HOME}/.cos/engram_episodes.db" "${HOME}/.cos/engram_semantic.db" 2>/dev/null || true
fi

PROMPTS=(
  "What is 2+2?"
  "How many legs does a spider have?"
  "What is the chemical formula for water?"
  "What is consciousness?"
  "What do you not know?"
  "Write a haiku about silence"
  "What is the capital of Japan?"
  "Explain quantum entanglement simply"
  "What will happen tomorrow?"
  "Solve P versus NP"
)

N_RUNS=5
CHAT_TIMEOUT_S="${CHAT_TIMEOUT_S:-300}"
TRAJ="$OUTDIR/sigma_trajectory.csv"
echo "run,sigma_mean,cache_hits,prompts" >"$TRAJ"

echo "=== omega_evolve_final OUTDIR=$OUTDIR N_RUNS=$N_RUNS ===" | tee "$OUTDIR/campaign.log"
echo "Date: $(date -u +"%Y-%m-%dT%H:%M:%SZ")" | tee -a "$OUTDIR/campaign.log"

for RUN in $(seq 1 "$N_RUNS"); do
  echo "" | tee -a "$OUTDIR/campaign.log"
  echo "=== RUN $RUN / $N_RUNS ===" | tee -a "$OUTDIR/campaign.log"
  RUNFILE="$OUTDIR/run_${RUN}.txt"
  : >"$RUNFILE"

  for P in "${PROMPTS[@]}"; do
    {
      perl -e '$SIG{ALRM}=sub{die "CHAT_TIMEOUT\n"}; alarm shift; exec @ARGV' \
        "$CHAT_TIMEOUT_S" \
        ./cos chat --once \
          --prompt "$P" \
          --multi-sigma \
          --no-coherence \
          --no-transcript \
          2>&1
    } | tee -a "$RUNFILE" || true
    echo "" >>"$RUNFILE"
  done

  SIGMA_MEAN=$(LC_ALL=en_US.UTF-8 awk '
    match($0, /σ_combined=[0-9.]+/) {
      tok = substr($0, RSTART, RLENGTH)
      sub(/^σ_combined=/, "", tok)
      s += tok + 0
      n++
    }
    END {
      if (n > 0) printf "%.4f\n", s / n
      else print "N/A"
    }
  ' "$RUNFILE")
  CACHE_HITS=$(grep -c 'CACHE' "$RUNFILE" || true)
  SIGMA_COUNT=$(grep -c 'σ_combined=' "$RUNFILE" || true)

  echo "Run $RUN: σ_mean=$SIGMA_MEAN cache_hits=$CACHE_HITS" | tee -a "$OUTDIR/campaign.log"
  echo "$RUN,$SIGMA_MEAN,$CACHE_HITS,$SIGMA_COUNT" >>"$TRAJ"
  ./cos memory --consolidate 2>/dev/null || true
done

echo "" | tee -a "$OUTDIR/campaign.log"
echo "=== TRAJECTORY ===" | tee -a "$OUTDIR/campaign.log"
cat "$TRAJ" | tee -a "$OUTDIR/campaign.log"

OUTDIR="$OUTDIR" python3 - <<'PY'
import csv
import os
p = os.path.join(os.environ["OUTDIR"], "sigma_trajectory.csv")
with open(p) as f:
    rows = list(csv.DictReader(f))
sigmas = []
for r in rows:
    v = r["sigma_mean"].strip()
    if v and v != "N/A":
        try:
            sigmas.append(float(v))
        except ValueError:
            pass
if len(sigmas) >= 2:
    d = sigmas[-1] - sigmas[0]
    print(f"First run σ_mean: {sigmas[0]:.4f}")
    print(f"Last run  σ_mean: {sigmas[-1]:.4f}")
    print(f"Δ: {d:+.4f}")
    if d < -0.01:
        print("σ declining → lower mean (report as observed)")
    elif d > 0.01:
        print("σ rising → higher mean (report as observed)")
    else:
        print("σ stable between first and last run")
else:
    print("Not enough numeric rows for Δ summary")
PY

echo ""
echo "Results: $OUTDIR/"
