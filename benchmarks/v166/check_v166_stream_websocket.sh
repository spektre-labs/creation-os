#!/usr/bin/env bash
#
# benchmarks/v166/check_v166_stream_websocket.sh
#
# Merge-gate for v166 σ-Stream.  Verifies the NDJSON stream
# structure a real WebSocket server would emit:
#
#   1. self-test
#   2. stream opens with a `start` frame carrying seed + n_tokens
#   3. every token frame has {kind, seq, token, sigma_product,
#      channels[8], audible_delay_ms}
#   4. sigma_product ∈ [0,1] per frame and equals geomean of
#      channels (within a tight tolerance)
#   5. baseline run (tau=0.95) completes without interrupt
#   6. injected burst run interrupts at the declared seq
#   7. audible_delay_ms = 40 + 400·sigma_product (voice hook)
#   8. determinism: two runs with same seed byte-identical
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v166_stream"
test -x "$BIN" || { echo "v166: binary not built"; exit 1; }

echo "[v166] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v166] (2..4+5) baseline stream parse"
OUT="$("$BIN" --run "alpha beta gamma delta epsilon" 0.95 -1 0x166AAAAABBBBCCCCULL || true)"
python3 - <<PY
import json, math, sys
lines = """$OUT""".strip().splitlines()
assert len(lines) >= 2, lines
frames = [json.loads(l) for l in lines]
assert frames[0]["kind"] == "start", frames[0]
assert "seed" in frames[0] and "n_tokens" in frames[0]
assert frames[-1]["kind"] in ("complete", "interrupted"), frames[-1]
# parse token frames
token_frames = [f for f in frames[1:-1] if f["kind"] == "token"]
assert len(token_frames) == frames[0]["n_tokens"], (len(token_frames), frames[0])
for f in token_frames:
    assert 0.0 <= f["sigma_product"] <= 1.0, f
    assert len(f["channels"]) == 8, f
    g = math.exp(sum(math.log(max(c, 1e-6)) for c in f["channels"]) / 8)
    assert abs(g - f["sigma_product"]) < 1e-3, (g, f)
    assert f["audible_delay_ms"] == 40 + int(400.0 * f["sigma_product"]), f
assert frames[-1]["kind"] == "complete", "baseline should not interrupt"
print("baseline stream ok")
PY

echo "[v166] (6) injected burst → interrupt"
OUT="$("$BIN" --run "one two three four five six" 0.50 3 0x166DEADBEEFULL || true)"
python3 - <<PY
import json
lines = """$OUT""".strip().splitlines()
frames = [json.loads(l) for l in lines]
inter = [f for f in frames if f["kind"] == "interrupted"]
assert len(inter) >= 2, inter  # the token with kind=interrupted AND the summary
assert frames[-1]["kind"] == "interrupted", frames[-1]
assert frames[-1]["interrupt_seq"] == 3, frames[-1]
# the triggering token frame:
trig = [f for f in frames if f["kind"] == "interrupted" and "token" in f]
assert trig and trig[0]["seq"] == 3, trig
assert trig[0]["sigma_product"] > 0.50, trig
print("interrupt path ok at seq=3")
PY

echo "[v166] (7) audible_delay formula already verified in (2..5)"

echo "[v166] (8) determinism"
A="$("$BIN" --run "repeat me please" 0.95 -1 0x1660CAFEF00DULL || true)"
B="$("$BIN" --run "repeat me please" 0.95 -1 0x1660CAFEF00DULL || true)"
[ "$A" = "$B" ] || { echo "v166: NDJSON not deterministic"; exit 8; }

echo "[v166] ALL CHECKS PASSED"
