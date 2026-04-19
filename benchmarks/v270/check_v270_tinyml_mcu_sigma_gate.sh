#!/usr/bin/env bash
#
# v270 σ-TinyML — merge-gate check.
#
# Contracts:
#   * self-test PASSES
#   * footprint envelope: sigma_measurement_bytes == 12,
#     code_flash_bytes <= 1024, ram_bytes_per_instance
#     <= 100, thumb2_instr_count <= 24, branchless
#   * 4 MCU targets canonical (cortex_m0_plus,
#     cortex_m4, cortex_m7, xtensa_esp32), all
#     supported and cpu_mhz > 0
#   * 3 sensors canonical (temperature, humidity,
#     pressure); sigma_local in [0,1]
#   * 4 fusion fixtures; decision matches σ_fusion vs
#     τ_fusion (0.30); >=1 TRANSMIT AND >=1 RETRY;
#     σ_fusion == max(σ_temp, σ_humidity, σ_pressure)
#   * 4 anomaly rows; anomaly == (σ > σ_baseline +
#     delta); >=1 true AND >=1 false
#   * 3 OTA rounds; every applied AND every
#     firmware_reflash == false
#   * sigma_tinyml in [0,1] AND == 0.0
#   * repeated binary invocations produce identical JSON
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v270_tinyml"
[ -x "$BIN" ] || { echo "v270: $BIN not built" >&2; exit 1; }

"$BIN" --self-test >/dev/null

OUT="$("$BIN")"
python3 - <<'PY' "$OUT"
import json, sys
d = json.loads(sys.argv[1])
assert d["kernel"] == "v270", d
assert d["chain_valid"] is True, d
assert abs(d["tau_fusion"] - 0.30) < 1e-6, d

fp = d["footprint"]
assert fp["sigma_measurement_bytes"] == 12, fp
assert fp["code_flash_bytes"]        <= 1024, fp
assert fp["ram_bytes_per_instance"]  <= 100, fp
assert fp["thumb2_instr_count"]      <= 24, fp
assert fp["branchless"] is True, fp
assert d["footprint_ok"] is True, d

T = d["targets"]
want_t = ["cortex_m0_plus","cortex_m4","cortex_m7","xtensa_esp32"]
assert [t["name"] for t in T] == want_t, T
for t in T:
    assert t["supported"] is True, t
    assert t["cpu_mhz"]   >  0,    t
assert d["n_targets_ok"] == 4, d

S = d["sensors"]
assert [x["name"] for x in S] == ["temperature","humidity","pressure"], S
for x in S:
    assert 0.0 <= x["sigma_local"] <= 1.0, x
assert d["n_sensors_ok"] == 3, d

tau = d["tau_fusion"]
ntx = nr = 0
for f in d["fusion"]:
    m = max(f["sigma_temp"], f["sigma_humidity"], f["sigma_pressure"])
    assert abs(f["sigma_fusion"] - m) < 1e-5, f
    exp = "TRANSMIT" if f["sigma_fusion"] <= tau else "RETRY"
    assert f["decision"] == exp, f
    if f["decision"] == "TRANSMIT": ntx += 1
    else:                           nr  += 1
assert ntx >= 1 and nr >= 1, (ntx, nr)
assert d["n_fusion_ok"] == 4, d
assert d["n_transmit"] == ntx, d
assert d["n_retry"]    == nr,  d

n_t = n_f = 0
for a in d["anomaly"]:
    exp = a["sigma_measured"] > a["sigma_baseline"] + a["delta"]
    assert a["anomaly"] == exp, a
    if a["anomaly"]: n_t += 1
    else:            n_f += 1
assert n_t >= 1 and n_f >= 1, (n_t, n_f)
assert d["n_anomaly_ok"]    == 4,  d
assert d["n_anomaly_true"]  == n_t, d
assert d["n_anomaly_false"] == n_f, d

for o in d["ota"]:
    assert o["applied"]          is True,  o
    assert o["firmware_reflash"] is False, o
assert d["n_ota_ok"] == 3, d

assert 0.0 <= d["sigma_tinyml"] <= 1.0, d
assert d["sigma_tinyml"] < 1e-6, d
PY

A="$("$BIN")"; B="$("$BIN")"
[ "$A" = "$B" ] || { echo "v270: non-deterministic" >&2; exit 1; }
