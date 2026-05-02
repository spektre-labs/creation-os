#!/usr/bin/env bash
#
# v277 σ-Distill-Runtime — merge-gate (pair + learn filter + domains + trajectory).
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v277_distill_runtime"
[ -x "$BIN" ] || { echo "v277: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v277"
assert d["teacher"] == "api-claude" and d["student"] == "bitnet-3B-local"
assert d["teacher"] != d["student"]
assert abs(d["tau_learn"] - 0.25) < 1e-5
assert abs(d["tau_domain"] - 0.30) < 1e-5
assert d["denominator"] == 19 and d["passing"] == 19
assert abs(d["sigma_distill"]) < 1e-5
assert d["manifest_closed"] is True

L = d["learn"]
assert len(L) == 4 and all(x["expect_ok"] for x in L)
dec = [x["decision"] for x in L]
assert "LEARN" in dec and "SKIP" in dec

D = d["domains"]
assert len(D) == 3 and all(x["expect_ok"] for x in D)
routes = [x["route"] for x in D]
assert "LOCAL_ONLY" in routes and "API" in routes

T = d["trajectory"]
assert len(T) == 4
for row in T:
    assert abs(row["api_share"] + row["local_share"] - 1.0) < 1e-3
apis = [row["api_share"] for row in T]
assert apis == sorted(apis, reverse=True)
locs = [row["local_share"] for row in T]
assert locs == sorted(locs)
costs = [row["cost"] for row in T]
assert costs == sorted(costs, reverse=True)
assert apis[0] >= 0.75 - 1e-5
assert apis[-1] <= 0.10 + 1e-5
assert d["learn_both_ok"] and d["domain_both_ok"] and d["traj_monotone_ok"]
assert d["chain_hash"].startswith("0x")
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v277: non-deterministic JSON" >&2; exit 1; }
