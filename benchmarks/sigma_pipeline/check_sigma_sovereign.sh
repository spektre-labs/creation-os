#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_sovereign
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"       >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"  >&2; exit 4; }

# Demo: 85 local @ €0.0001 + 15 cloud @ €0.012.
#   sovereign_fraction = 0.85
#   eur_local_total    = 0.0085
#   eur_cloud_total    = 0.18
#   eur_per_call       = (0.0085 + 0.18) / 100 = 0.001885
#   monthly @ 100/day  = 0.001885 × 100 × 30 = 5.655
#   cloud-only monthly = 0.012 × 100 × 30   = 36.0
#   saved_pct          = 100 × (1 − 5.655 / 36.0) ≈ 84.29
grep -q '"min_sovereign_fraction":0.8500' <<<"$OUT" || { echo "min frac"      >&2; exit 5; }
grep -q '"n_local":85'                    <<<"$OUT" || { echo "n_local"       >&2; exit 6; }
grep -q '"n_cloud":15'                    <<<"$OUT" || { echo "n_cloud"       >&2; exit 7; }
grep -q '"sovereign_fraction":0.8500'     <<<"$OUT" || { echo "sov frac"      >&2; exit 8; }
grep -q '"verdict":"HYBRID"'              <<<"$OUT" || { echo "verdict"       >&2; exit 9; }
grep -q '"eur_per_call":0.001885'         <<<"$OUT" || { echo "eur_per_call"  >&2; exit 10; }
grep -q '"monthly_eur_at_100":5.6550'     <<<"$OUT" || { echo "monthly"       >&2; exit 11; }
grep -q '"cloud_only_monthly_eur":36.0000'<<<"$OUT" || { echo "cloud_only"    >&2; exit 12; }
grep -Eq '"saved_pct":84\.[0-9]{2}'       <<<"$OUT" || { echo "saved_pct"     >&2; exit 13; }
grep -q '"bytes_sent_cloud":15360'        <<<"$OUT" || { echo "bytes sent"    >&2; exit 14; }
grep -q '"bytes_recv_cloud":61440'        <<<"$OUT" || { echo "bytes recv"    >&2; exit 15; }

echo "check-sigma-sovereign: PASS (85 local + 15 cloud → HYBRID; €5.66/mo vs €36.00 cloud-only, ~84% saved)"
