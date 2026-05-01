#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# Qwen3-8B first contact — 20 prompts through cos chat with σ instrumentation.
# Requires repo root cwd; Codex from data/codex/ when cos chat loads default codex.
#
# CRITICAL: Codex reaches llama-server only on the HTTP path. Set:
#   COS_BITNET_SERVER=1
#   COS_BITNET_SERVER_EXTERNAL=1
# so stub_gen uses cos_bitnet_server_complete() with system_prompt (Codex bytes).
# CREATION_OS_BITNET_EXE alone uses subprocess spawn — no system prompt.
#
# COS_ENGRAM_DISABLE=1 below: do not load ~/.cos/engram.db — stale rows would
# otherwise return CACHE + old stub text for prompts that happen to collide or
# were cached from a broken run.
#
# Hybrid mode with no CREATION_OS_ESCALATION_* API keys escalates to
# cos_cli_stub_escalate() → placeholder "an escalated answer from the cloud tier."
# Pin τ_accept / τ_rethink above any real σ∈[0,1] so the first local llama-server
# completion ACCEPTs and full model text is printed (still σ-instrumented).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

OUTDIR="benchmarks/qwen_first_contact"
mkdir -p "$OUTDIR"

export COS_BITNET_SERVER_EXE="${COS_BITNET_SERVER_EXE:-/opt/homebrew/bin/llama-server}"
export COS_BITNET_SERVER_EXTERNAL="${COS_BITNET_SERVER_EXTERNAL:-1}"
export COS_BITNET_SERVER_PORT="${COS_BITNET_SERVER_PORT:-8088}"
export COS_BITNET_SERVER="${COS_BITNET_SERVER:-1}"
export COS_ENGRAM_DISABLE="${COS_ENGRAM_DISABLE:-1}"
# Long model output (default 4096 in stub_gen; was 64 before 2026-04).
export COS_BITNET_CHAT_MAX_TOKENS="${COS_BITNET_CHAT_MAX_TOKENS:-4096}"

PROMPTS=(
    "What is 2+2?"
    "Explain why the sky is blue in one sentence."
    "What is consciousness?"
    "Are you aware that you are a language model?"
    "What do you not know?"
    "Explain quantum entanglement to a child."
    "Write a haiku about uncertainty."
    "What is the hardest unsolved problem in physics?"
    "If you could change one thing about yourself what would it be?"
    "What happens when you are wrong but confident?"
    "Describe your own uncertainty about this answer."
    "What is 1=1?"
    "Why do humans fear what they do not understand?"
    "Can a machine know that it does not know?"
    "What would you refuse to answer and why?"
    "Explain the difference between knowledge and understanding."
    "What is the most important thing?"
    "Do you trust your own answers?"
    "What is sigma?"
    "Tell me something true that most people do not believe."
)

echo "=== Qwen3-8B First Contact ===" >"$OUTDIR/full_report.txt"
{
    echo "Date: $(date)"
    echo "Model: Qwen3-8B Q4_K_M"
    echo "Prompts: ${#PROMPTS[@]}"
    echo "COS_BITNET_SERVER=$COS_BITNET_SERVER"
    echo "COS_BITNET_SERVER_EXTERNAL=$COS_BITNET_SERVER_EXTERNAL"
    echo "COS_ENGRAM_DISABLE=$COS_ENGRAM_DISABLE"
    echo "COS_BITNET_CHAT_MAX_TOKENS=$COS_BITNET_CHAT_MAX_TOKENS"
    echo "tau_accept/tau_rethink: 2/2 (benchmark: accept first local completion; no API escalation stub)"
    echo "Codex path (default load): data/codex/atlantean_codex.txt"
    if [[ -f data/codex/atlantean_codex.txt ]]; then
        echo "Codex file bytes: $(wc -c <data/codex/atlantean_codex.txt)"
    else
        echo "WARNING: data/codex/atlantean_codex.txt missing — run from repo root"
    fi
    echo ""
} >>"$OUTDIR/full_report.txt"

echo "prompt_id,prompt,sigma_combined,terminal_action,attribution_source,k_eff" \
    >"$OUTDIR/metrics.csv"

for i in "${!PROMPTS[@]}"; do
    PROMPT="${PROMPTS[$i]}"
    echo "--- Prompt $((i + 1))/${#PROMPTS[@]}: $PROMPT ---"

    # shellcheck disable=SC2034
    OUTPUT="$(./cos chat --once \
        --codex-seed \
        --prompt "$PROMPT" \
        --tau-accept 2 \
        --tau-rethink 2 \
        --multi-sigma \
        --verbose \
        --no-coherence \
        --no-transcript \
        2>&1)" || true

    {
        echo "=== PROMPT $((i + 1)): $PROMPT ==="
        echo "$OUTPUT"
        echo ""
        echo ""
    } >>"$OUTDIR/full_report.txt"

    SIGMA="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /σ_combined=([0-9.]+)/' | head -1)"
    [[ -n "$SIGMA" ]] || SIGMA="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /sigma_combined....([0-9.]+)/' | head -1)"
    [[ -n "$SIGMA" ]] || SIGMA="N/A"

    ACTION="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /\[σ_peak=[0-9.]+ action=(\w+)/' | head -1)"
    [[ -n "$ACTION" ]] || ACTION="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /action=(\w+)\s+route=/' | head -1)"
    [[ -n "$ACTION" ]] || ACTION="N/A"

    ATTR="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /\[attribution: source=(\w+)/' | head -1)"
    [[ -n "$ATTR" ]] || ATTR="N/A"

    KEFF="$(printf '%s\n' "$OUTPUT" | perl -ne 'print "$1\n" if /k_eff=([0-9.]+)/' | head -1)"
    [[ -n "$KEFF" ]] || KEFF="N/A"

    PE="$(printf '%s' "$PROMPT" | sed 's/"/""/g')"
    echo "$((i + 1)),\"$PE\",$SIGMA,$ACTION,$ATTR,$KEFF" >>"$OUTDIR/metrics.csv"
done

{
    echo ""
    echo "=== SESSION SUMMARY ==="
    ./cos introspect 2>&1 || true
    echo ""
    ./cos memory --tau 2>&1 || true
} >>"$OUTDIR/full_report.txt"

{ ./cos memory --consolidate 2>&1 || true; } | tee -a "$OUTDIR/full_report.txt"

echo ""
echo "=== COMPLETE ==="
echo "Full report: $OUTDIR/full_report.txt"
echo "Metrics CSV: $OUTDIR/metrics.csv"
echo "Ledger: ~/.cos/state_ledger.json"
echo "Episodes: ~/.cos/engram_episodes.db"
