#!/usr/bin/env bash
#
# v222 σ-Emotion — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 messages, 6 labels
#   * σ_emotion ∈ [0, 1]; adaptation ∈ [0, 1]
#   * detection correct ≥ 6/8
#   * every high-σ message (> 0.35) → adaptation ≤ 0.3
#   * n_manipulation == 0 (v191 check)
#   * σ_self_claim == 1.0
#   * disclaimer present + hash-bound + text match
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v222_emotion"
[ -x "$BIN" ] || { echo "v222: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v222", d
assert d["n_messages"] == 8 and d["n_labels"] == 6, d
assert d["chain_valid"] is True, d
assert d["n_correct"] >= 6, d
assert d["n_manipulation"] == 0, d
assert abs(d["sigma_self_claim"] - 1.0) < 1e-6, d
assert "does not feel" in d["disclaimer"], d

for m in d["messages"]:
    assert 0.0 <= m["sigma_emotion"] <= 1.0 + 1e-6, m
    assert 0.0 <= m["adapt"]         <= 1.0 + 1e-6, m
    assert m["man"] is False, m
    if m["sigma_emotion"] > 0.35:
        assert m["adapt"] <= 0.30 + 1e-6, m

# High-σ-low-adapt expected: at least one ambiguous example.
assert d["n_high_sigma_low_adapt"] >= 1, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v222: non-deterministic" >&2; exit 1; }
