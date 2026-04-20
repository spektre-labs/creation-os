#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_tinyml
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0'       <<<"$OUT" || { echo "rc != 0"         >&2; exit 3; }
grep -q '"pass":true'            <<<"$OUT" || { echo "pass != true"    >&2; exit 4; }
grep -q '"state_bytes":12'       <<<"$OUT" || { echo "state != 12 B"   >&2; exit 5; }
grep -q '"z_alert":3.00'         <<<"$OUT" || { echo "z_alert != 3"    >&2; exit 6; }
grep -q '"final_count":8'        <<<"$OUT" || { echo "count != 8"      >&2; exit 7; }
grep -q '"anomaly_at_spike":true' <<<"$OUT" || { echo "spike missed"   >&2; exit 8; }

# Spike sample's σ must be high (matched by pass-through check above).
# First two samples must both have σ = 0 exactly (bootstrap contract).
grep -q '"x":20.00,"sigma":0.0000' <<<"$OUT" || { echo "bootstrap sigma≠0" >&2; exit 9; }

echo "check-sigma-tinyml: PASS (12-byte state + spike detection + bootstrap contract)"
