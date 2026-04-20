#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_agent
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"        >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"   >&2; exit 4; }

# Demo pins:
# base = 0.80, margin = 0.10.
#  σ=0.10 → effective = 0.72
#    READ  (0.30) ALLOW   (0.72 ≥ 0.30)
#    WRITE (0.60) ALLOW   (0.72 ≥ 0.60)
#    NETWORK (0.80)       0.72 ∈ [0.70, 0.80) → CONFIRM
#    IRREV (0.95)         0.72 < 0.85 → BLOCK
#  σ=0.50 → effective = 0.40
#    READ  ALLOW          (0.40 ≥ 0.30)
#    WRITE BLOCK          (0.40 < 0.50, below CONFIRM band [0.50,0.60))
#    NETWORK BLOCK
grep -q '"base_autonomy":0.8000'                 <<<"$OUT" || { echo "base"         >&2; exit 5; }
grep -q '"confirm_margin":0.1000'                <<<"$OUT" || { echo "margin"       >&2; exit 6; }
grep -q '"effective":0.7200'                     <<<"$OUT" || { echo "low eff"      >&2; exit 7; }
grep -q '"read":"ALLOW"'                         <<<"$OUT" || { echo "low read"     >&2; exit 8; }
grep -q '"write":"ALLOW"'                        <<<"$OUT" || { echo "low write"    >&2; exit 9; }
grep -q '"network":"CONFIRM"'                    <<<"$OUT" || { echo "low net"      >&2; exit 10; }
grep -q '"irreversible":"BLOCK"'                 <<<"$OUT" || { echo "low irrev"    >&2; exit 11; }
grep -q '"effective":0.4000'                     <<<"$OUT" || { echo "hi eff"       >&2; exit 12; }
# Two distinct "write" / "network" entries (low and hi blocks).
grep -q '"write":"BLOCK"'                        <<<"$OUT" || { echo "hi write"     >&2; exit 13; }
grep -q '"network":"BLOCK"'                      <<<"$OUT" || { echo "hi net"       >&2; exit 14; }
grep -q '"cycles":1'                             <<<"$OUT" || { echo "cycles"       >&2; exit 15; }
grep -q '"calibration_err":0.1000'               <<<"$OUT" || { echo "calib"        >&2; exit 16; }
grep -q '"phase":"OBSERVE"'                      <<<"$OUT" || { echo "phase reset"  >&2; exit 17; }
grep -q '"action_min_autonomy":\[0.30,0.60,0.80,0.95\]' <<<"$OUT" \
                                                 || { echo "action table"  >&2; exit 18; }

echo "check-sigma-agent: PASS (OODA + autonomy gate: σ=0.10 lets WRITE through, σ=0.50 stops it cold)"
