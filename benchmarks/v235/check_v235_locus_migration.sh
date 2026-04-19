#!/usr/bin/env bash
#
# v235 σ-Locus — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * n_nodes == 4, n_topics == 3
#   * per topic: locus_after == argmin(sigma_after), locus_before
#     == argmin(sigma_before), and sigma_min_after == sigma_after
#     at locus_after
#   * every sigma value in [0, 1]; sigma_locus_unity ∈ [0, 1]
#   * n_migrations >= 1
#   * split_brain: chain_len_a > 0 AND chain_len_b > 0;
#     winner_partition = 0 iff chain_len_a >= chain_len_b else 1;
#     loser_flagged_fork == true
#   * sigma_locus_mean_unity ∈ [0, 1]
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v235_locus"
[ -x "$BIN" ] || { echo "v235: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v235", d
assert d["chain_valid"] is True, d
assert d["n_nodes"] == 4 and d["n_topics"] == 3, d

for tp in d["topics"]:
    sb = tp["sigma_before"]; sa = tp["sigma_after"]
    assert len(sb) == 4 and len(sa) == 4, tp
    for v in sb + sa:
        assert 0.0 <= v <= 1.0, tp
    lb = min(range(4), key=lambda i: sb[i])
    la = min(range(4), key=lambda i: sa[i])
    assert tp["locus_before"] == lb, tp
    assert tp["locus_after"]  == la, tp
    assert abs(tp["sigma_min_after"] - sa[la]) < 1e-6, tp
    assert 0.0 <= tp["sigma_locus_unity"] <= 1.0, tp

assert d["n_migrations"] >= 1, d
assert 0.0 <= d["sigma_locus_mean_unity"] <= 1.0, d

sb = d["split_brain"]
assert sb["chain_len_a"] > 0 and sb["chain_len_b"] > 0, sb
expected = 0 if sb["chain_len_a"] >= sb["chain_len_b"] else 1
assert sb["winner_partition"] == expected, sb
assert sb["loser_flagged_fork"] is True, sb
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v235: non-deterministic" >&2; exit 1; }
