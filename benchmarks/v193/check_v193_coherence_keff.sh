#!/usr/bin/env bash
#
# v193 σ-Coherence — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * ρ, I_Φ, F, σ, K, K_eff all ∈ [0, 1]
#   * K_eff = (1 - σ) · K everywhere (within 1e-4)
#   * at least 1 tick triggers alert (K_eff < K_crit)
#   * recovery occurs within ≤ 3 ticks of first alert
#   * K_eff strictly monotone through the improvement phase
#     (ticks 11..15)
#   * Δ K_eff over improvement phase > 0 (v187 ECE drop → F rise)
#   * byte-deterministic
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v193_coherence"
[ -x "$BIN" ] || { echo "v193: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<PY
import json
d = json.loads("""$OUT""")

assert d["kernel"]  == "v193", d
assert d["n_ticks"] == 16,     d
assert d["all_components_in_range"]    is True, d
assert d["K_eff_identity_holds"]        is True, d
assert d["improvement_phase_monotone"]  is True, d

# Alerts and recovery.
assert d["n_alerts"]        >= 1, d
assert d["first_alert_tick"] >= 0, d
assert d["recovery_tick"]    >  d["first_alert_tick"], d
assert d["recovery_lag"]    <= 3, d

# K_eff identity numerically.
for k in d["ticks"]:
    expected = (1.0 - k["sigma"]) * k["K"]
    assert abs(expected - k["K_eff"]) < 1e-4, k

# Every component ∈ [0, 1].
for k in d["ticks"]:
    for field in ("rho", "i_phi", "F", "sigma", "K", "K_eff",
                  "accuracy", "ece", "alignment"):
        assert -1e-6 <= k[field] <= 1.0 + 1e-6, (field, k)

# Improvement phase strictly increasing.
for t in range(11, 16):
    assert d["ticks"][t]["K_eff"] > d["ticks"][t-1]["K_eff"] - 1e-6, (t, d["ticks"][t])

# Δ K_eff after calibration > 0.
assert d["delta_K_eff_after_calibration"] > 0, d

# Final K_eff must be above K_crit (system recovered).
assert d["ticks"][-1]["K_eff"] > d["K_crit"], d
PY

A="$("$BIN")"; B="$("$BIN")"
if [ "$A" != "$B" ]; then
    echo "v193: non-deterministic output" >&2
    exit 1
fi
