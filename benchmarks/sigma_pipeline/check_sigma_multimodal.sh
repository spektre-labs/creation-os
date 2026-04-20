#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_multimodal
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"         >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"    >&2; exit 4; }

# Canonical σ demos — pin exact values.
grep -q '"text_sigma":0.4200'          <<<"$OUT" || { echo "text_sigma"          >&2; exit 5; }
grep -q '"code_ok":0.1000'             <<<"$OUT" || { echo "code_ok"             >&2; exit 6; }
grep -q '"code_bad":1.0000'            <<<"$OUT" || { echo "code_bad"            >&2; exit 7; }
grep -q '"schema_ok":0.1000'           <<<"$OUT" || { echo "schema_ok"           >&2; exit 8; }
grep -q '"schema_one_missing":0.4000'  <<<"$OUT" || { echo "schema_missing"      >&2; exit 9; }
grep -q '"image_similar":0.0500'       <<<"$OUT" || { echo "image_similar"       >&2; exit 10; }
grep -q '"image_mismatch":0.9000'      <<<"$OUT" || { echo "image_mismatch"      >&2; exit 11; }
grep -q '"audio":0.2500'               <<<"$OUT" || { echo "audio"               >&2; exit 12; }

# Labels table.
for lbl in TEXT CODE STRUCTURED IMAGE AUDIO; do
  grep -q "\"$lbl\"" <<<"$OUT" || { echo "missing label $lbl" >&2; exit 13; }
done

echo "check-sigma-multimodal: PASS (5 modalities + 8 canonical σ pins)"
