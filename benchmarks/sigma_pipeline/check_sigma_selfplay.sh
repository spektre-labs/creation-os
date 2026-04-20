#!/usr/bin/env bash
# σ-Selfplay smoke test (S1): proposer/solver/σ-verifier + bands + proconductor.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_selfplay
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Canonical demo config.
grep -q '"tau_verify":0.40'         <<<"$OUT" || { echo "tau_verify" >&2; exit 5; }
grep -q '"difficulty_target":0.40'  <<<"$OUT" || { echo "target"     >&2; exit 6; }

# Round 1 — TOO_EASY (σ_a=0.10).  d_in=0.40, raised toward 0.4475.
grep -q '"d_in":0.40,"sigma_q":0.1000,"sigma_a":0.1000,"label":"TOO_EASY","verified":true,"d_new":0.4475' <<<"$OUT" \
    || { echo "easy round" >&2; exit 7; }

# Round 2 — LEARNING (σ_a=0.40).  d stays at target 0.40.
grep -q '"d_in":0.40,"sigma_q":0.1500,"sigma_a":0.4000,"label":"LEARNING","verified":false,"d_new":0.4000' <<<"$OUT" \
    || { echo "learning round" >&2; exit 8; }

# Round 3 — TOO_HARD (σ_a=0.80).  d_in=0.80, dropped to 0.6850.
grep -q '"d_in":0.80,"sigma_q":0.2000,"sigma_a":0.8000,"label":"TOO_HARD","verified":false,"d_new":0.6850' <<<"$OUT" \
    || { echo "hard round" >&2; exit 9; }

# Stats.
grep -q '"n":3'           <<<"$OUT" || { echo "n"         >&2; exit 10; }
grep -q '"n_verified":1'  <<<"$OUT" || { echo "verified"  >&2; exit 11; }
grep -q '"n_too_easy":1'  <<<"$OUT" || { echo "too_easy"  >&2; exit 12; }
grep -q '"n_learning":1'  <<<"$OUT" || { echo "learning"  >&2; exit 13; }
grep -q '"n_too_hard":1'  <<<"$OUT" || { echo "too_hard"  >&2; exit 14; }
grep -q '"mean_sigma_q":0.1500' <<<"$OUT" || { echo "mean_q" >&2; exit 15; }
grep -q '"mean_sigma_a":0.4333' <<<"$OUT" || { echo "mean_a" >&2; exit 16; }

# Proconductor: self σ=0.05, other σ=0.70, margin 0.20 → overconfident.
grep -q '"sigma_self":0.0500'        <<<"$OUT" || { echo "self"        >&2; exit 17; }
grep -q '"sigma_other":0.7000'       <<<"$OUT" || { echo "other"       >&2; exit 18; }
grep -q '"overconfident":true'       <<<"$OUT" || { echo "overconf"    >&2; exit 19; }

echo "check-sigma-selfplay: PASS (proposer + solver + σ-verifier + bands + proconductor)"
