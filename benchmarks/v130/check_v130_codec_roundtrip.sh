#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Merge-gate for v130 σ-Codec.
#
# Validates:
#   1. self-test green (FP4 + σ-pack + PQ + context)
#   2. FP4 round-trip RMSE acceptable on 1024 synthetic lora values
#   3. σ-profile 8-byte packer bounded by 1/255 max error
#   4. σ-aware context: uncertain chunks keep more content than
#      confident ones (asserts the σ-allocation behaviour)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN="$ROOT/creation_os_v130_codec"
if [ ! -x "$BIN" ]; then
    echo "check-v130: building creation_os_v130_codec..." >&2
    make -s creation_os_v130_codec
fi

echo "check-v130-codec-roundtrip: --self-test"
"$BIN" --self-test >/dev/null

echo "check-v130-codec-roundtrip: FP4 round-trip on 1024 synthetic values"
FP4_OUT="$("$BIN" --fp4-roundtrip 1024 | head -1)"
echo "  $FP4_OUT"
python3 - <<EOF
import re, json
m = re.search(r'\{.*\}', """$FP4_OUT""")
d = json.loads(m.group(0))
assert d["n"] == 1024, d
assert d["rel_rmse"] < 0.15, f"rel_rmse {d['rel_rmse']} >= 0.15"
assert d["rel_rmse"] > 0.0,  d
print(f"  FP4 rel_rmse={d['rel_rmse']:.6f} ok")
EOF

echo "check-v130-codec-roundtrip: σ-profile pack ≤ 1/255"
SP_OUT="$("$BIN" --sigma-pack "0.0,0.25,0.33,0.5,0.66,0.75,0.9,1.0" | head -1)"
echo "  $SP_OUT"
python3 - <<EOF
import re, json
m = re.search(r'\{.*\}', """$SP_OUT""")
d = json.loads(m.group(0))
assert d["packed_bytes"] == 8
assert d["max_err"] <= 1.0/255.0 + 1e-6, f"max_err {d['max_err']} too large"
print(f"  σ-pack max_err={d['max_err']:.8f} ok")
EOF

echo "check-v130-codec-roundtrip: σ-aware context — uncertain > confident"
SPEC="0.10|This is a very confident sentence about basic arithmetic lessons kids learn.;0.85|Uncertain: the quantum-gravity unification remains an open and disputed research question.;0.10|Another simple confident statement about the calendar system."
CTX_OUT="$("$BIN" --context-compress 180 "$SPEC")"
echo "  $CTX_OUT"
HDR="$(echo "$CTX_OUT" | head -1)"
BODY="$(echo "$CTX_OUT" | tail -n +2)"
python3 - <<EOF
import json, re
m = re.search(r'\{.*\}', """$HDR""")
d = json.loads(m.group(0))
body = """$BODY"""
assert d["chunks"] == 3, d
assert d["output_bytes"] <= d["budget"] + 20, d  # overhead allowance
assert d["output_bytes"] <  d["input_bytes"], "must compress"
# uncertain chunk's key word must survive; confident ones may be truncated
assert "quantum" in body, f"uncertain content missing: {body!r}"
print(f"  context compressed {d['input_bytes']}→{d['output_bytes']}, uncertain preserved")
EOF

echo "check-v130-codec-roundtrip: OK"
