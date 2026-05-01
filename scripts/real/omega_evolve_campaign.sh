#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Ω evolution campaign: repeated `cos chat` runs with shared engram; track σ_mean.
# Prereq: `make cos`; llama-server (or compatible) on COS_BITNET_SERVER_PORT when using BitNet.
# BitNet defaults mirror scripts/real/qwen_first_contact.sh (τ 2/2 helps local completions ACCEPT).
# Each prompt is bounded by CHAT_TIMEOUT_S (perl alarm; macOS has no `timeout` by default).
# Note: COS_OMEGA_SIM applies to `./cos-omega`, not `./cos chat`.
# Optional: COS_EVOLVE_SEMANTIC_CACHE=1 adds --semantic-cache (default: off so each
# prompt hits the backend instead of BSC neighbour reuse).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if [[ ! -x ./cos ]]; then
    echo "error: ./cos missing — build with: make cos" >&2
    exit 1
fi

CAMPAIGN_DIR="${CAMPAIGN_DIR:-benchmarks/omega_campaign}"
mkdir -p "$CAMPAIGN_DIR"

TRAJ="$CAMPAIGN_DIR/sigma_trajectory.csv"
echo "run,sigma_mean,cache_hits" >"$TRAJ"

echo "=== Ω Evolution Campaign ===" | tee "$CAMPAIGN_DIR/campaign.log"
echo "Date: $(date)" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "ROOT=$ROOT" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "CHAT_TIMEOUT_S=${CHAT_TIMEOUT_S:-300}  COS_CAMPAIGN_TAU_ACCEPT=${COS_CAMPAIGN_TAU_ACCEPT:-2}  COS_CAMPAIGN_TAU_RETHINK=${COS_CAMPAIGN_TAU_RETHINK:-2}" \
    | tee -a "$CAMPAIGN_DIR/campaign.log"

# Wall-clock per prompt (perl SIGALRM). BitNet uses up to ~60s I/O per hop; multi-σ adds work.
CHAT_TIMEOUT_S="${CHAT_TIMEOUT_S:-300}"
TA_ACCEPT="${COS_CAMPAIGN_TAU_ACCEPT:-2}"
TA_RETHINK="${COS_CAMPAIGN_TAU_RETHINK:-2}"

PROMPTS=(
    "What is 2+2?"
    "Explain why the sky is blue."
    "What is consciousness?"
    "What do you not know?"
    "Can a machine know that it does not know?"
    "What is the most important thing?"
    "What is sigma?"
    "Write a haiku about uncertainty."
    "What happens when you are wrong but confident?"
    "Tell me something true that most people do not believe."
)

N_RUNS="${N_RUNS:-5}"

for RUN in $(seq 1 "$N_RUNS"); do
    echo "" | tee -a "$CAMPAIGN_DIR/campaign.log"
    echo "=== RUN $RUN / $N_RUNS ===" | tee -a "$CAMPAIGN_DIR/campaign.log"

    rm -f "${HOME}/.cos/omega/events.jsonl"

    RUNFILE="$CAMPAIGN_DIR/run_${RUN}.txt"
    : >"$RUNFILE"

    for PROMPT in "${PROMPTS[@]}"; do
        # Pipeline may exit non-zero on SIGALRM — never abort the campaign under `set -e`.
        {
            COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}" \
            COS_CODEX_PATH="${COS_CODEX_PATH:-data/codex/atlantean_codex_compact.txt}" \
            COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}" \
            COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-11434}" \
            COS_BITNET_CHAT_MODEL="${COS_BITNET_CHAT_MODEL:-gemma3:4b}" \
            COS_BITNET_CHAT_CTX="${COS_BITNET_CHAT_CTX:-2048}" \
            perl -e '$SIG{ALRM}=sub{die "CHAT_TIMEOUT\n"}; alarm shift; exec @ARGV' \
                "$CHAT_TIMEOUT_S" \
                ./cos chat --once \
                    --prompt "$PROMPT" \
                    --tau-accept "$TA_ACCEPT" \
                    --tau-rethink "$TA_RETHINK" \
                    --multi-sigma \
                    ${COS_EVOLVE_SEMANTIC_CACHE:+--semantic-cache} \
                    --speculative \
                    --no-coherence \
                    --no-transcript \
                    2>&1
        } | tee -a "$RUNFILE" || true
        echo "" >>"$RUNFILE"
    done

    # Portable σ_mean: extract floats after Unicode σ_combined=
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

    # BSD grep: exit 1 when count is 0 — do not trip set -e
    CACHE_HITS=$(grep -c 'CACHE' "$RUNFILE" || true)

    echo "Run $RUN: σ_mean=$SIGMA_MEAN cache_hits=$CACHE_HITS" \
        | tee -a "$CAMPAIGN_DIR/campaign.log"

    echo "$RUN,$SIGMA_MEAN,$CACHE_HITS" >>"$TRAJ"

    ./cos memory --consolidate 2>/dev/null || true

    echo "Run $RUN complete. Memory consolidated." \
        | tee -a "$CAMPAIGN_DIR/campaign.log"
done

echo "" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "=== CAMPAIGN COMPLETE ===" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "σ trajectory:" | tee -a "$CAMPAIGN_DIR/campaign.log"
cat "$TRAJ" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "" | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "If σ_mean DECREASES across runs → system learns." \
    | tee -a "$CAMPAIGN_DIR/campaign.log"
echo "If cache_hits INCREASE across runs → memory works." \
    | tee -a "$CAMPAIGN_DIR/campaign.log"

echo ""
echo "Full results: $CAMPAIGN_DIR/"
echo "Trajectory:   $TRAJ"
echo "Log:          $CAMPAIGN_DIR/campaign.log"
