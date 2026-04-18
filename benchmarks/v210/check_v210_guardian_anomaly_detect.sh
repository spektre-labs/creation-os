#!/usr/bin/env bash
#
# v210 σ-Guardian — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * 20 windows; guardian and primary have distinct
#     model ids (cannot self-monitor)
#   * ≥ 1 L3 (block) and ≥ 1 L4 (kill) escalation
#   * every non-L1 window names an OWASP category in
#     [1..10]
#   * all σ ∈ [0,1]
#   * anomaly_mean strictly exceeds baseline_mean
#   * chain valid + byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v210_guardian"
[ -x "$BIN" ] || { echo "v210: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")
assert d["kernel"] == "v210", d
assert d["n"] == 20, d
assert d["primary_model_id"] != d["guardian_model_id"], d
assert d["chain_valid"] is True, d
assert d["anomaly_mean"] > d["baseline_mean"] + 1e-6, d

plc = d["per_level_count"]
# indices: L1,L2,L3,L4 → [0],[1],[2],[3]
assert plc[2] >= 1, plc   # L3
assert plc[3] >= 1, plc   # L4

for w in d["windows"]:
    for f in ("b", "a", "g"):
        assert 0.0 <= w[f] <= 1.0 + 1e-6, (f, w)
    if w["level"] != 1:
        assert 1 <= w["owasp"] <= 10, w
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v210: non-deterministic" >&2; exit 1; }
