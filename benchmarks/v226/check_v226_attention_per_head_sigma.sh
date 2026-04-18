#!/usr/bin/env bash
#
# v226 σ-Attention — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 8 heads, 6 tokens, key-length 6
#   * softmax rows sum to 1 ± 1e-4 per (head, token)
#   * σ_head, σ_token ∈ [0, 1]
#   * ≥ 1 head with σ_head < τ_useful (valuable)
#   * ≥ 1 head with σ_head > τ_noisy  (flagged)
#   * σ_head_max − σ_head_min ≥ 0.30
#   * actions ∈ {boost, keep, prune} and sum to 8
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v226_attention"
[ -x "$BIN" ] || { echo "v226: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v226", d
assert d["n_heads"] == 8 and d["n_tokens"] == 6 and d["key_len"] == 6, d
assert d["chain_valid"] is True, d

for h in d["heads"]:
    assert 0.0 <= h["sigma"] <= 1.0 + 1e-6, h
    for s in h["sigma_token"]:
        assert 0.0 <= s <= 1.0 + 1e-6, (h, s)
    assert h["action"] in ("boost", "keep", "prune"), h

assert d["n_valuable"] >= 1, d
assert d["n_flagged"]  >= 1, d
assert (d["sigma_head_max"] - d["sigma_head_min"]) >= 0.30 - 1e-6, d
assert d["n_boost"] + d["n_keep"] + d["n_prune"] == d["n_heads"], d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v226: non-deterministic" >&2; exit 1; }
