#!/usr/bin/env bash
# σ-Grounding smoke test (A4): verdicts + FNV provenance.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_grounding
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

grep -q '"tau_claim":0.4000'    <<<"$OUT" || { echo "tau claim"  >&2; exit 5; }
grep -q '"tau_source":0.4000'   <<<"$OUT" || { echo "tau source" >&2; exit 6; }
grep -q '"n_sources":2'         <<<"$OUT" || { echo "n_sources"  >&2; exit 7; }
grep -q '"n_claims":3'          <<<"$OUT" || { echo "n_claims"   >&2; exit 8; }

# Demo bucket counts: Paris claim GROUNDED, Nitrogen claim CONFLICTED,
# Octopus claim UNSUPPORTED.
grep -q '"buckets":{"grounded":1,"unsupported":1,"conflicted":1,"skipped":0}' <<<"$OUT" \
    || { echo "buckets" >&2; exit 9; }
grep -q '"ground_rate":0.3333'   <<<"$OUT" || { echo "rate"      >&2; exit 10; }

grep -q '"claim0":{"verdict":"GROUNDED"'    <<<"$OUT" || { echo "claim0" >&2; exit 11; }
grep -q '"claim1":{"verdict":"CONFLICTED"'  <<<"$OUT" || { echo "claim1" >&2; exit 12; }
grep -q '"claim2":{"verdict":"UNSUPPORTED"' <<<"$OUT" || { echo "claim2" >&2; exit 13; }

# FNV-1a-64 empty-string hash pin.
grep -q '"empty_hash":"cbf29ce484222325"' <<<"$OUT" \
    || { echo "empty fnv" >&2; exit 14; }

# Claim0 matched a real source → source_hash ≠ 0.
grep -q '"claim0":{"verdict":"GROUNDED","source_hash":"0000000000000000"' <<<"$OUT" \
    && { echo "claim0 should have source_hash != 0" >&2; exit 15; }
# Claim2 unsupported → source_hash == 0.
grep -q '"claim2":{"verdict":"UNSUPPORTED","source_hash":"0000000000000000"' <<<"$OUT" \
    || { echo "claim2 source hash" >&2; exit 16; }

echo "check-sigma-grounding: PASS (3 verdict classes + FNV provenance)"
