#!/usr/bin/env bash
#
# NEXT-4 smoke test: σ-Stream — per-token σ streaming inference.
#
# Pinned facts on the canonical "quantum entanglement" demo stream
# (τ_rethink = 0.50):
#   - self_test_rc == 0
#   - n_tokens  == 9
#   - n_rethink == 2   (σ = 0.55 at idx 4, σ = 0.60 at idx 6)
#   - sigma_sum  == 1.5200, sigma_max == 0.6000
#   - rethink_at_idx == [4, 6]
#   - text_len == 71 (exact concatenation of demo tokens)
#   - neither token- nor text-buffer truncated
#   - two back-to-back runs are byte-identical
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_stream"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT_A="$($BIN)"
OUT_B="$($BIN)"

[[ "$OUT_A" == "$OUT_B" ]] \
    || { echo "FAIL: stream output not deterministic" >&2; exit 3; }

for fact in \
    '"self_test_rc":0' \
    '"tau_rethink":0.5000' \
    '"n_tokens":9' \
    '"n_rethink":2' \
    '"sigma_sum":1.5200' \
    '"sigma_max":0.6000' \
    '"rethink_at_idx":[4,6]' \
    '"truncated_tokens":false' \
    '"truncated_text":false' \
    '"text_len":71' \
    '"pass":true'
do
    grep -q -F "$fact" <<<"$OUT_A" \
        || { echo "FAIL: missing '$fact' in stream output" >&2
             echo "$OUT_A"; exit 4; }
done

python3 - "$OUT_A" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
assert j["n_tokens"] == len(j["sigma_per_token"])
assert j["n_rethink"] == len(j["rethink_at_idx"])

for i, s in enumerate(j["sigma_per_token"]):
    assert 0.0 <= s <= 1.0, s
    if s >= j["tau_rethink"] - 1e-6:
        assert i in j["rethink_at_idx"], \
            f"σ={s} ≥ τ but {i} not in rethink_at_idx"
    else:
        assert i not in j["rethink_at_idx"]

assert j["sigma_max"] == max(j["sigma_per_token"])
assert abs(j["sigma_sum"] - sum(j["sigma_per_token"])) < 1e-4
mean = j["sigma_sum"] / j["n_tokens"]
assert abs(j["sigma_mean"] - mean) < 1e-4
print("check-sigma-stream: PASS (deterministic, per-token σ bounded, "
      "rethink fires where σ ≥ τ, " + str(j["n_rethink"]) + " rethinks)")
PY
