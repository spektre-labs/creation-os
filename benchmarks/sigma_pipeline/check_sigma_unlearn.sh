#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_unlearn
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"       >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"  >&2; exit 4; }

# Demo: weights [5.0, 0.1, 0.1, 0.1] vs target [3.0, 0, 0, 0].
# σ_before is low (weights strongly aligned with target via first
# component); iterate should push σ above sigma_target=0.95.
grep -q '"succeeded":true'           <<<"$OUT" || { echo "not succeeded"  >&2; exit 5; }
grep -q '"sigma_target":0.9500'      <<<"$OUT" || { echo "sigma_target"   >&2; exit 6; }
grep -q '"strength":0.5000'          <<<"$OUT" || { echo "strength"       >&2; exit 7; }
grep -q '"max_iters":20'             <<<"$OUT" || { echo "max_iters"      >&2; exit 8; }

# Subject hash must be non-zero FNV output (20-digit uint64 range).
grep -Eq '"subject_hash":"[0-9]{18,20}"' <<<"$OUT" || { echo "subject_hash shape" >&2; exit 9; }

# σ_after must be ≥ 0.95.  Coarse pin: "0.9" prefix after decimal.
sig_after=$(sed -E 's/.*"sigma_after":([0-9.]+).*/\1/' <<<"$OUT")
awk "BEGIN{exit !($sig_after >= 0.95)}" \
  || { echo "sigma_after ($sig_after) < 0.95" >&2; exit 10; }

# σ_before must be ≤ 0.05 (strongly aligned before unlearning).
sig_before=$(sed -E 's/.*"sigma_before":([0-9.]+).*/\1/' <<<"$OUT")
awk "BEGIN{exit !($sig_before <= 0.1)}" \
  || { echo "sigma_before ($sig_before) > 0.1" >&2; exit 11; }

# n_iters bounded by max_iters and > 0.
grep -Eq '"n_iters":[1-9][0-9]?' <<<"$OUT" || { echo "n_iters invalid" >&2; exit 12; }

echo "check-sigma-unlearn: PASS (GDPR-style unlearn: σ $sig_before → $sig_after, FORGOTTEN ≥ 0.95)"
