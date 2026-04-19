#!/usr/bin/env bash
#
# v251 σ-Marketplace — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * registry_url == "registry.creation-os.dev"
#   * exactly 5 kernels in canonical order
#     (medical-v1, legal-v1, finance-v1, science-v1,
#      teach-pro-v1), each with n_deps > 0 and
#     sigma_quality = mean(axes) ± 1e-4
#   * install: medical-v1 installed, deps_resolved ==
#     n_deps, install_ok
#   * compose: medical-v1 + legal-v1 → medical-legal,
#     sigma_compatibility in [0,1] and < tau_compat
#     (0.50), compose_ok
#   * publish contract: 4 items in order
#     (merge_gate_green, sigma_profile_attached,
#      docs_attached, scsl_license_attached), every
#     required AND met
#   * sigma_marketplace in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v251_marketplace"
[ -x "$BIN" ] || { echo "v251: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v251", d
assert d["chain_valid"] is True, d
assert d["registry_url"] == "registry.creation-os.dev", d
assert d["n_kernels"] == 5, d

kn = [k["name"] for k in d["kernels"]]
assert kn == ["medical-v1","legal-v1","finance-v1",
               "science-v1","teach-pro-v1"], kn
for k in d["kernels"]:
    assert k["n_deps"] > 0, k
    mean = (k["sigma_calibration"] + k["benchmark_score"] +
            k["user_rating"] + k["audit_trail"]) / 4.0
    assert abs(k["sigma_quality"] - mean) < 1e-4, k
    assert 0.0 <= k["sigma_quality"] <= 1.0, k
assert d["n_kernels_ok"] == 5, d

ins = d["install"]
assert ins["kernel"] == "medical-v1", ins
assert ins["deps_resolved"] == ins["n_deps"], ins
assert ins["installed"] is True, ins
assert d["install_ok"] is True, d

cmp = d["compose"]
assert cmp["left"]     == "medical-v1", cmp
assert cmp["right"]    == "legal-v1",   cmp
assert cmp["composed"] == "medical-legal", cmp
assert abs(cmp["tau_compat"] - 0.50) < 1e-6, cmp
assert 0.0 <= cmp["sigma_compatibility"] < cmp["tau_compat"], cmp
assert cmp["compose_ok"] is True, cmp
assert d["compose_ok"] is True, d

pi = [p["item"] for p in d["publish"]]
assert pi == ["merge_gate_green","sigma_profile_attached",
               "docs_attached","scsl_license_attached"], pi
for p in d["publish"]:
    assert p["required"] is True, p
    assert p["met"] is True, p
assert d["n_publish_met"] == 4, d

assert 0.0 <= d["sigma_marketplace"] <= 1.0, d
assert d["sigma_marketplace"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v251: non-deterministic" >&2; exit 1; }
