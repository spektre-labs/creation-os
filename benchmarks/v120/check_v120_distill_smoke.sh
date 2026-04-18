#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v120 σ-Distill merge-gate smoke.
#
# What this check enforces:
#   - self-test green (synthetic 8-row corpus)
#   - selector keeps exactly the σ-targeted rows
#     (σ_big < 0.30 AND σ_small > 0.50)
#   - output JSONL carries the teacher response, not the student's
#   - manifest JSON has the required keys + counts
#
# v120.0 is the selector.  Training the LoRA adapter with MLX is
# v120.1 — see docs/v120/README.md.  The merge-gate does not attempt
# training so nothing here needs MLX or weights.

set -euo pipefail

BIN=${BIN:-./creation_os_v120_distill}
if [ ! -x "$BIN" ]; then
    echo "check-v120: $BIN not found; run \`make creation_os_v120_distill\`" >&2
    exit 1
fi

echo "check-v120: self-test"
"$BIN" --self-test

tmpdir=$(mktemp -d)
trap 'rm -rf "$tmpdir"' EXIT

cat > "$tmpdir/in.jsonl" <<'EOF'
{"id":"a","prompt":"2+2","big_response":"4","small_response":"4?","sigma_big":0.10,"sigma_small":0.80}
{"id":"b","prompt":"capital of France","big_response":"Paris","small_response":"Lyon","sigma_big":0.15,"sigma_small":0.70}
{"id":"c","prompt":"who wrote Hamlet","big_response":"Shakespeare","small_response":"Dickens","sigma_big":0.25,"sigma_small":0.60}
{"id":"d","prompt":"sky color","big_response":"blue","small_response":"blue","sigma_big":0.15,"sigma_small":0.30}
{"id":"e","prompt":"both sure","big_response":"x","small_response":"y","sigma_big":0.60,"sigma_small":0.80}
EOF

echo "check-v120: select"
manifest=$("$BIN" --select "$tmpdir/in.jsonl" "$tmpdir/out.jsonl")
echo "$manifest"

# 3 σ-targeted rows, 2 rejected.
echo "$manifest" | grep -q '"rows_kept":3' \
    || { echo "expected rows_kept:3, got $manifest"; exit 1; }
echo "$manifest" | grep -q '"rows_read":5' \
    || { echo "expected rows_read:5, got $manifest"; exit 1; }
echo "$manifest" | grep -q '"tau_big_low":'    || { echo "manifest missing τ_big";   exit 1; }
echo "$manifest" | grep -q '"tau_small_high":' || { echo "manifest missing τ_small"; exit 1; }

kept_lines=$(wc -l < "$tmpdir/out.jsonl" | tr -d ' ')
[ "$kept_lines" -eq 3 ] || { echo "expected 3 output lines, got $kept_lines"; exit 1; }

grep -q 'Paris'        "$tmpdir/out.jsonl" || { echo "teacher response 'Paris' missing";       exit 1; }
grep -q 'Shakespeare'  "$tmpdir/out.jsonl" || { echo "teacher response 'Shakespeare' missing"; exit 1; }
grep -q 'Lyon'         "$tmpdir/out.jsonl" && { echo "student response 'Lyon' leaked";         exit 1; }

# Custom thresholds: very permissive keeps everything σ_big<1.0 and
# σ_small>0, i.e. all 5 rows.
manifest=$("$BIN" --select "$tmpdir/in.jsonl" "$tmpdir/out2.jsonl" 0.99 0.0)
echo "$manifest" | grep -q '"rows_kept":5' \
    || { echo "permissive thresholds should keep all 5"; exit 1; }

echo "check-v120: OK (σ-targeted selector + SFT manifest contract)"
