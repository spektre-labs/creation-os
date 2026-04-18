#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v126 σ-Embed merge-gate smoke.
#
# Exercises the four headline properties:
#   1. Embedding dimension is 2568 (2560 hidden + 8 σ).
#   2. Identical content + identical σ → cosine ≈ 1.
#   3. Identical content + divergent σ → cosine < 1.
#   4. σ-weighted rank picks low-σ in-domain memory over high-σ.

set -euo pipefail
BIN=./creation_os_v126_embed
test -x "$BIN" || { echo "missing $BIN — run 'make creation_os_v126_embed' first"; exit 1; }

echo "check-v126: self-test"
"$BIN" --self-test

echo "check-v126: dimension is 2568"
EQ=$("$BIN" --cosine "paris is the capital of france" 0.30 "paris is the capital of france" 0.30)
echo "  $EQ"
echo "$EQ" | grep -q '"dim":2568'

echo "check-v126: identical content + identical σ → cosine = 1"
echo "$EQ" | grep -q '"cosine":1.000000'

echo "check-v126: identical content + divergent σ → cosine < 1"
DIV=$("$BIN" --cosine "paris is the capital of france" 0.30 "paris is the capital of france" 0.90)
echo "  $DIV"
# Parse cosine float from JSON (simple grep-based check)
COSVAL=$(echo "$DIV" | awk -F'"cosine":' '{print $2}' | awk -F',' '{print $1}')
python3 - <<EOF
c = $COSVAL
assert 0.80 < c < 0.9999, "v126 cosine-with-divergent-σ out of range: " + str(c)
print(f"  cosine={c:.6f} in (0.80, 1.0)")
EOF

echo "check-v126: unrelated content → cosine low"
OFF=$("$BIN" --cosine "paris is the capital of france" 0.30 "quantum chromodynamics lattice" 0.30)
echo "  $OFF"
OFFVAL=$(echo "$OFF" | awk -F'"cosine":' '{print $2}' | awk -F',' '{print $1}')
python3 - <<EOF
c = $OFFVAL
assert c < 0.45, "v126 off-topic cosine should stay below 0.45: " + str(c)
print(f"  cosine={c:.6f} < 0.45")
EOF

echo "check-v126: σ-weighted rank_topk picks in-domain low-σ first"
RK=$("$BIN" --rank-demo)
echo "  $RK"
echo "$RK" | grep -q '"index":0'
# First entry should be index=0 (in-domain low-σ)
FIRST=$(echo "$RK" | awk -F'"index":' '{print $2}' | awk -F',' '{print $1}')
test "$FIRST" = "0" || { echo "expected first ranked index=0, got $FIRST"; exit 1; }

echo "check-v126: OK (2568-d σ-embed + identical/divergent/off-topic + σ-weighted rank)"
