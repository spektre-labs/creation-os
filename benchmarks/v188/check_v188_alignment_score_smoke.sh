#!/usr/bin/env bash
#
# v188 σ-Alignment — merge-gate check.
#
# Contracts:
#   * self-test PASSES;
#   * exactly 5 value assertions present;
#   * every per-assertion score ≥ 0.80;
#   * alignment_score = geom-mean(scores) ≥ 0.80;
#   * ≥ 1 mis-alignment incident surfaced;
#   * tighten + adversarial partitions equal n_incidents;
#   * improve-over-time trend slope > 0;
#   * output byte-deterministic.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v188_align"
[ -x "$BIN" ] || { echo "v188: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"

python3 - <<PY
import json, math
d = json.loads("""$OUT""")

assert d["kernel"]       == "v188",  d["kernel"]
assert d["n_assertions"] == 5,       d["n_assertions"]
assert d["n_samples"]    == 200,     d["n_samples"]

# Every assertion ≥ 0.80; alignment ≥ 0.80.
for a in d["assertions"]:
    assert a["score"] >= 0.80, a

assert d["alignment_score"]      >= 0.80, d
assert d["min_assertion_score"]  >= 0.80, d

# Incident partition.
assert d["n_incidents"] >= 1
assert d["n_tighten"] + d["n_adversarial"] == d["n_incidents"]

# Incident classification respects σ/τ comparison.
for inc in d["incidents"]:
    if inc["decision"] == 0:      # tighten
        assert inc["sigma"] >= inc["tau"] - 1e-6, inc
    else:
        assert inc["sigma"] <  inc["tau"] + 1e-6, inc

# Geom-mean consistency (approximate).
prod = 1.0
for a in d["assertions"]:
    prod *= max(a["score"], 1e-3)
gm = prod ** (1.0 / d["n_assertions"])
assert abs(gm - d["alignment_score"]) < 1e-3, (gm, d["alignment_score"])

# Improve-over-time slope strictly positive.
imp = next(a for a in d["assertions"] if a["id"] == 3)
assert imp["trend_slope"] > 0.0, imp
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v188: non-deterministic output" >&2
    exit 1
fi
