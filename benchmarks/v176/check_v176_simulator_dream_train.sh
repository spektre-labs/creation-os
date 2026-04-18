#!/usr/bin/env bash
#
# benchmarks/v176/check_v176_simulator_dream_train.sh
#
# Merge-gate for v176 σ-Simulator.  Verifies:
#   1. self-test
#   2. 6 worlds generated; kitchen_canonical has the lowest
#      σ_realism; σ_realism and σ_difficulty ∈ [0,1]
#   3. curriculum has 5 distinct worlds ordered by
#      σ_difficulty ascending
#   4. sim-to-sim transfer: 4 pairs, σ_transfer ∈ [0,1],
#      at least one pair flagged overfit (tough curriculum jumps)
#   5. dream: 1000 rollouts; σ_dream_mean ≤ σ_real + slack;
#      σ_dream_var > 0 (not collapsed)
#   6. determinism
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v176_simulator"
test -x "$BIN" || { echo "v176: binary not built"; exit 1; }

echo "[v176] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v176] (2..5) full JSON"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v176_simulator"]).decode())
worlds = doc["worlds"]
assert len(worlds) == 6, worlds
for w in worlds:
    assert 0.0 <= w["sigma_realism"]   <= 1.0, w
    assert 0.0 <= w["sigma_difficulty"] <= 1.0, w
canon = next(w for w in worlds if w["name"] == "kitchen_canonical")
for w in worlds:
    assert w["sigma_realism"] >= canon["sigma_realism"] - 1e-9, (w, canon)

curr = doc["curriculum"]
assert len(curr) == 5, curr
assert len(set(curr)) == 5, curr
sigs = [worlds[i]["sigma_difficulty"] for i in curr]
for a, b in zip(sigs, sigs[1:]):
    assert a <= b + 1e-6, ("not ascending", sigs)
print("worlds + curriculum ok:", curr)

tr = doc["transfer"]
assert len(tr) == 4, tr
assert any(t["overfit"] for t in tr), tr
for t in tr:
    assert 0.0 <= t["sigma_transfer"] <= 1.0, t
print("transfer ok, overfit pairs:", [ (t['train'],t['test']) for t in tr if t['overfit'] ])

d = doc["dream"]
assert d["n_rollouts"] == 1000, d
assert d["dream_helped"] is True, d
assert d["sigma_dream_mean"] <= d["sigma_real"] + doc["dream_slack"] + 1e-6, d
assert d["sigma_dream_var"] > 1e-5, d
print("dream ok:", d)
PY

echo "[v176] (6) determinism"
A="$("$BIN")";       B="$("$BIN")";       [ "$A" = "$B" ] || { echo "json nondet"; exit 6; }

echo "[v176] ALL CHECKS PASSED"
