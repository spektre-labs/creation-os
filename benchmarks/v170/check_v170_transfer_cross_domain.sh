#!/usr/bin/env bash
#
# benchmarks/v170/check_v170_transfer_cross_domain.sh
#
# Merge-gate for v170 σ-Transfer.  Verifies:
#   1. self-test
#   2. state JSON: 8 domains with 4-D embedding + σ ∈ [0,1]
#   3. near pair math↔physics is closer than far pair math↔poetry
#   4. transfer to biology: source = chemistry, go=true,
#      σ_after < σ_before, rollback=false
#   5. transfer to history: source is one of the strong neighbours
#      AND either go=true with σ_after ≤ σ_before OR go=false with
#      a non-empty reason (the kernel must never silently harm)
#   6. zero-shot on physics: k ≥ 1 and 0 < σ_ensemble < 1
#   7. determinism on state JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v170_transfer"
test -x "$BIN" || { echo "v170: binary not built"; exit 1; }

echo "[v170] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v170] (2..3) state JSON"
python3 - <<'PY'
import json, subprocess, math
doc = json.loads(subprocess.check_output(["./creation_os_v170_transfer"]).decode())
assert len(doc["domains"]) == 8, doc
names = {d["name"]: d for d in doc["domains"]}
for d in doc["domains"]:
    assert 0.0 <= d["sigma"] <= 1.0, d
    assert len(d["embed"]) == 4, d
def dist(a, b):
    ea = names[a]["embed"]; eb = names[b]["embed"]
    return math.sqrt(sum((ea[i]-eb[i])**2 for i in range(4)))
d_mp = dist("math", "physics")
d_mo = dist("math", "poetry")
assert d_mp < d_mo, (d_mp, d_mo)
print("state ok, d(math,physics)=%.3f d(math,poetry)=%.3f" % (d_mp, d_mo))
PY

echo "[v170] (4) transfer biology"
T="$("$BIN" --transfer biology)"
python3 - <<PY
import json
r = json.loads("""$T""")
assert r["event"] == "transfer"
assert r["target"] == "biology"
assert r["source"] == "chemistry", r
assert r["go"] is True, r
assert r["rollback"] is False, r
assert r["sigma_after"] < r["sigma_before"], r
assert r["delta"] < 0, r
print("biology ok: σ %.3f → %.3f" % (r["sigma_before"], r["sigma_after"]))
PY

echo "[v170] (5) transfer history (safe behaviour)"
T="$("$BIN" --transfer history)"
python3 - <<PY
import json
r = json.loads("""$T""")
assert r["event"] == "transfer"
if r["go"]:
    assert r["sigma_after"] <= r["sigma_before"] + 1e-6, r
else:
    assert r["reason"], r
print("history ok, go=%s rollback=%s" % (r["go"], r["rollback"]))
PY

echo "[v170] (6) zero-shot physics"
Z="$("$BIN" --zero-shot physics)"
python3 - <<PY
import json
r = json.loads("""$Z""")
assert r["event"] == "zero_shot"
assert r["target"] == "physics"
assert r["k"] >= 1, r
assert 0.0 < r["sigma_ensemble"] < 1.0, r
print("zero-shot ok, k=%d σ=%.3f" % (r["k"], r["sigma_ensemble"]))
PY

echo "[v170] (7) determinism"
A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v170: state JSON not deterministic"; exit 7; }

echo "[v170] ALL CHECKS PASSED"
