#!/usr/bin/env bash
# σ-Session smoke test (A5): eviction + trend + save/load round-trip.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_session
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"      >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true" >&2; exit 4; }

# Cap 4, 6 appends → 4 live, 2 evicted.
grep -q '"cap":4'              <<<"$OUT" || { echo "cap"      >&2; exit 5; }
grep -q '"count":4'            <<<"$OUT" || { echo "count"    >&2; exit 6; }
grep -q '"evicted":2'          <<<"$OUT" || { echo "evicted"  >&2; exit 7; }
grep -q '"next_turn_id":6'     <<<"$OUT" || { echo "next_id"  >&2; exit 8; }

# Surviving σs should be {0.40, 0.20, 0.10, 0.05}.  Sorted ascending.
grep -q '"surviving_sigmas":\[0.0500,0.1000,0.2000,0.4000\]' <<<"$OUT" \
    || { echo "survivors" >&2; exit 9; }
grep -q '"sigma_mean":0.1875'  <<<"$OUT" || { echo "sigma_mean" >&2; exit 10; }
grep -q '"sigma_min":0.0500'   <<<"$OUT" || { echo "sigma_min"  >&2; exit 11; }
grep -q '"sigma_max":0.4000'   <<<"$OUT" || { echo "sigma_max"  >&2; exit 12; }

# Save/load round-trip: reload count + mean match.
grep -q '"save_load_rc":0'     <<<"$OUT" || { echo "save_load rc" >&2; exit 13; }
grep -q '"reload_count":4'     <<<"$OUT" || { echo "reload"   >&2; exit 14; }
grep -q '"reload_sigma_mean":0.1875' <<<"$OUT" || { echo "reload mean" >&2; exit 15; }

echo "check-sigma-session: PASS (σ-priority eviction + trend + checksum round-trip)"
