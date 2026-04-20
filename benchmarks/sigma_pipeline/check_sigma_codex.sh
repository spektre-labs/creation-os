#!/usr/bin/env bash
# Smoke test for the Atlantean Codex loader (I0).
#
# Pins the invariants `cos chat`, `cos benchmark` and the end-to-end
# integration tests rely on:
#   * self_test_rc == 0 / pass == true
#   * full Codex: ≥ 33 chapters, invariant "1 = 1" present, non-zero
#     FNV-1a-64 hash
#   * seed Codex: is_seed=1, invariant present, non-zero hash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_codex
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

[[ -f data/codex/atlantean_codex.txt ]] \
    || { echo "missing data/codex/atlantean_codex.txt" >&2; exit 3; }
[[ -f data/codex/codex_seed.txt ]] \
    || { echo "missing data/codex/codex_seed.txt" >&2; exit 4; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 5; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 6; }

# Full Codex must load, cover ≥ 33 chapters, carry 1=1, emit a
# non-zero FNV-1a-64 hash.
grep -q '"full":{"rc":0,'         <<<"$OUT" || { echo "full rc"       >&2; exit 7; }
grep -qE '"chapters":(3[3-9]|[4-9][0-9]|[1-9][0-9]{2,})' \
    <<<"$(sed -n 's/.*"full":{\([^}]*\)}.*/\1/p' <<<"$OUT")" \
    || { echo "chapters < 33" >&2; exit 8; }
grep -q '"full":{[^}]*"invariant":true'    <<<"$OUT" || { echo "no 1=1"      >&2; exit 9; }
grep -qE '"full":{[^}]*"hash":"[0-9a-f]{16}"' <<<"$OUT" || { echo "hash full"   >&2; exit 10; }

# Seed: is_seed flag set, invariant present, hash emitted.
grep -q '"seed":{"rc":0,'          <<<"$OUT" || { echo "seed rc"      >&2; exit 11; }
grep -q '"seed":{[^}]*"invariant":true'   <<<"$OUT" || { echo "seed 1=1"   >&2; exit 12; }
grep -q '"seed":{[^}]*"is_seed":1'        <<<"$OUT" || { echo "is_seed"    >&2; exit 13; }
grep -qE '"seed":{[^}]*"hash":"[0-9a-f]{16}"' <<<"$OUT" || { echo "hash seed" >&2; exit 14; }

echo "check-codex: PASS (full + seed load; chapters ≥ 33; 1=1; hash stable)"
