#!/usr/bin/env bash
# Smoke test for σ-Engram primitive.
set -euo pipefail

cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_engram
[[ -x "$BIN" ]] || { echo "missing binary: $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'   <<<"$OUT" || { echo "self_test_rc != 0" >&2; exit 3; }
grep -q '"pass":true'        <<<"$OUT" || { echo "pass != true"      >&2; exit 4; }
# Demo: 3 puts, 2 hits + 1 miss, count=3, cap=8.
grep -q '"cap":8'            <<<"$OUT" || { echo "cap != 8"          >&2; exit 5; }
grep -q '"count":3'          <<<"$OUT" || { echo "count != 3"        >&2; exit 6; }
grep -q '"hit1":1'           <<<"$OUT" || { echo "hit1 != 1"         >&2; exit 7; }
grep -q '"hit2":1'           <<<"$OUT" || { echo "hit2 != 1"         >&2; exit 8; }
grep -q '"miss":0'           <<<"$OUT" || { echo "miss != 0"         >&2; exit 9; }
grep -q '"n_get_hit":2'      <<<"$OUT" || { echo "n_get_hit != 2"    >&2; exit 10; }
grep -q '"n_get_miss":1'     <<<"$OUT" || { echo "n_get_miss != 1"   >&2; exit 11; }
grep -q '"n_put":3'          <<<"$OUT" || { echo "n_put != 3"        >&2; exit 12; }
grep -q '"n_evict":0'        <<<"$OUT" || { echo "n_evict != 0"      >&2; exit 13; }

echo "check-sigma-engram: PASS (binary self-test rc=0; 3 puts / 2 hits / 1 miss / 0 evictions)"
