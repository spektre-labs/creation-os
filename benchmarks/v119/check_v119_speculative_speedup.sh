#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v119 σ-Speculative merge-gate smoke.
#
# What this check enforces:
#   - self-test green
#   - σ-adaptive γ: σ=0 ⇒ γ ≥ 6, σ=0.9 ⇒ γ = γ_min = 2
#   - confident synthetic stream: acceptance_rate > 0.60 (every 7th
#     token is a target mismatch so the theoretical ceiling is ~6/7)
#   - uncertain synthetic stream: σ-gate saves ≥ 90% of target calls
#
# Throughput (tokens/sec) on real target+draft GGUFs is measured in
# v119.1 (llama.cpp bridge); the v119.0 gate guards only the policy
# layer and its σ contract, per docs/CLAIM_DISCIPLINE.md.

set -euo pipefail

BIN=${BIN:-./creation_os_v119_speculative}
if [ ! -x "$BIN" ]; then
    echo "check-v119: $BIN not found; run \`make creation_os_v119_speculative\`" >&2
    exit 1
fi

echo "check-v119: self-test"
"$BIN" --self-test

echo "check-v119: adaptive γ edges"
g_lo=$("$BIN" --gamma 0.0)
g_hi=$("$BIN" --gamma 0.9)
[ "$g_lo" -ge 6 ] || { echo "γ(σ=0.0) expected ≥ 6, got $g_lo"; exit 1; }
[ "$g_hi" -eq 2 ] || { echo "γ(σ=0.9) expected 2, got $g_hi";  exit 1; }

tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

echo "check-v119: confident stream acceptance"
"$BIN" --simulate 256 0.10 > "$tmp"
grep -q '"acceptance_rate":0\.[6-9]' "$tmp" \
  || grep -q '"acceptance_rate":1\.' "$tmp" \
  || { cat "$tmp"; echo "acceptance_rate too low"; exit 1; }
grep -q '"tokens_sigma_gated":0' "$tmp" \
  || { cat "$tmp"; echo "σ-gates fired on confident stream"; exit 1; }

echo "check-v119: uncertain stream target-save"
"$BIN" --simulate 256 0.90 > "$tmp"
grep -q '"target_invocations":0' "$tmp" \
  || { cat "$tmp"; echo "uncertain stream invoked target"; exit 1; }
grep -qE '"target_save_rate":(1\.|0\.9[0-9])' "$tmp" \
  || { cat "$tmp"; echo "target_save < 0.90 on uncertain stream"; exit 1; }

echo "check-v119: OK (policy + σ-adaptive γ + σ-gate contract)"
