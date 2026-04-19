#!/usr/bin/env bash
#
# v244 σ-Package — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * exactly 4 platforms in order {macOS, linux, docker, windows};
#     every platform has a non-empty install_cmd and manifest_ok
#   * minimal profile: exactly the seed quintet [29, 101, 106, 124, 148]
#   * full profile: n_kernels >= 243
#   * first-run: 4 steps in order SEED -> GROWING -> CHECKING -> READY
#     with strictly-ascending ticks
#   * updates: at least 3 accepted AND at least 1 rejected
#     (rejected has sigma_update > tau_update == 0.50)
#   * rollback_ok == true (installed_version after rollback step
#     equals its declared `to` string)
#   * sigma_package in [0,1] AND == 0.0
#   * deterministic: repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v244_package"
[ -x "$BIN" ] || { echo "v244: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v244", d
assert d["chain_valid"] is True, d
assert d["n_platforms"] == 4, d

names = [p["name"] for p in d["platforms"]]
assert names == ["macOS", "linux", "docker", "windows"], names
for p in d["platforms"]:
    assert p["manifest_ok"] is True, p
    assert len(p["install_cmd"]) > 0, p

assert d["n_platforms_ok"] == 4, d

mk = d["minimal"]["kernels"]
assert mk == [29, 101, 106, 124, 148], mk
assert d["minimal"]["n_kernels"] == 5, d
assert d["full"]["n_kernels"] >= 243, d

fr = d["firstrun"]
assert [x["state"] for x in fr] == ["SEED","GROWING","CHECKING","READY"], fr
for i in range(1, len(fr)):
    assert fr[i]["tick"] > fr[i-1]["tick"], fr

tau = d["tau_update"]
assert abs(tau - 0.50) < 1e-6, d
assert d["n_updates_accepted"] >= 3, d
assert d["n_updates_rejected"] >= 1, d
for u in d["updates"]:
    if u["accepted"]:
        assert u["sigma_update"] <= tau + 1e-6, u
    else:
        assert u["sigma_update"] >  tau - 1e-6, u

last = d["updates"][-1]
assert last["accepted"] is True, last
assert d["installed_version"] == last["to"], (d, last)
assert d["rollback_ok"] is True, d

assert 0.0 <= d["sigma_package"] <= 1.0, d
assert d["sigma_package"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v244: non-deterministic" >&2; exit 1; }
