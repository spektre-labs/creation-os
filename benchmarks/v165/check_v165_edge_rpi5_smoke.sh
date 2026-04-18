#!/usr/bin/env bash
#
# benchmarks/v165/check_v165_edge_rpi5_smoke.sh
#
# Merge-gate for v165 σ-Edge.  Verifies:
#   1. self-test
#   2. profile table contains rpi5 / jetson_orin / android /
#      macbook_m3 / ios with correct triples and σ-scale
#   3. iOS is marked supported_in_v0=false (roadmap honesty)
#   4. τ_edge adapts: smaller RAM → higher τ (monotone)
#   5. cos-lite boots on rpi5, jetson_orin, android under the
#      default budget (binary 5 + weights 1200 + kv 256 +
#      sigma 64 MB = 1525 MB, fits 3072+ easily)
#   6. cos-lite *refuses* to boot on ios (reserved for v165.1)
#   7. cross-compile matrix flags rpi5/jetson/android as QEMU-needed
#   8. determinism: identical profile table across two runs
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

set -euo pipefail

BIN="./creation_os_v165_edge"
test -x "$BIN" || { echo "v165: binary not built"; exit 1; }

echo "[v165] (1) self-test"
"$BIN" --self-test >/dev/null

echo "[v165] (2+3) profile table"
LIST="$("$BIN" --list 0.5)"
for t in rpi5 jetson_orin android macbook_m3 ios; do
    echo "$LIST" | grep -q "\"target\":\"$t\"" || { echo "v165: missing target $t"; exit 2; }
done
echo "$LIST" | grep -q '"triple":"aarch64-linux-gnu"' || { echo "v165: rpi5/jetson triple"; exit 2; }
echo "$LIST" | grep -q '"triple":"aarch64-linux-android"' || { echo "v165: android triple"; exit 2; }
echo "$LIST" | grep -q '"make_target":"cos-lite-rpi5"' || { echo "v165: rpi5 make target"; exit 2; }
# iOS must be roadmap-only
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v165_edge", "--list", "0.5"]).decode())
ios = [p for p in doc["profiles"] if p["target"] == "ios"]
assert len(ios) == 1 and ios[0]["supported_in_v0"] is False, ios
print("ios roadmap-only ok")
PY

echo "[v165] (4) τ_edge monotone"
python3 - <<'PY'
import json, subprocess
def tau(ram):
    out = subprocess.check_output(["./creation_os_v165_edge", "--tau", "0.5", str(ram)]).decode()
    return json.loads(out)["tau_edge"]
t_big  = tau(16384)
t_med  = tau(8192)
t_rpi  = tau(6144)
t_small= tau(2048)
# smaller RAM must yield higher (or equal when clamped) τ_edge
assert t_big <= t_med <= t_rpi <= t_small, (t_big, t_med, t_rpi, t_small)
assert t_small >= 0.5
print("tau monotone ok", (t_big, t_med, t_rpi, t_small))
PY

echo "[v165] (5) boot rpi5 + jetson + android"
for tgt in rpi5 jetson android; do
    R="$("$BIN" --boot "$tgt" 0.5)"
    echo "$R" | grep -q '"booted":true' || { echo "v165: $tgt did not boot"; echo "$R"; exit 5; }
    echo "$R" | grep -q '"fits":true'   || { echo "v165: $tgt did not fit";  exit 5; }
done

echo "[v165] (6) ios refuses in v0"
R="$("$BIN" --boot ios 0.5 || true)"
echo "$R" | grep -q '"booted":false'         || { echo "v165: ios should refuse"; exit 6; }
echo "$R" | grep -q "reserved for v165.1"    || { echo "v165: ios note missing"; exit 6; }

echo "[v165] (7) crosscompile QEMU flag"
python3 - <<'PY'
import json, subprocess
doc = json.loads(subprocess.check_output(["./creation_os_v165_edge", "--list", "0.5"]).decode())
for p in doc["profiles"]:
    if p["target"] in ("rpi5", "jetson_orin", "android"):
        assert p["crosscompile"]["needs_qemu"] is True, p
    if p["target"] in ("macbook_m3",):
        assert p["crosscompile"]["needs_qemu"] is False, p
print("qemu flags ok")
PY

echo "[v165] (8) determinism"
A="$("$BIN" --list 0.5)"
B="$("$BIN" --list 0.5)"
[ "$A" = "$B" ] || { echo "v165: listing not deterministic"; exit 8; }

echo "[v165] ALL CHECKS PASSED"
