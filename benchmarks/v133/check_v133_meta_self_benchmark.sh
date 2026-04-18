#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Merge-gate for v133 σ-Meta.
#
# Validates:
#   1. self-test green (history + slope + meta-σ + diagnose + bench)
#   2. --rising-demo flags regression_detected
#   3. --noisy-demo  flags calibration_drift (meta-σ > τ_meta)
#   4. --diagnose maps highest channel to the correct cause label
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN="$ROOT/creation_os_v133_meta"
if [ ! -x "$BIN" ]; then
    echo "check-v133: building creation_os_v133_meta..." >&2
    make -s creation_os_v133_meta
fi

echo "check-v133-meta-self-benchmark: --self-test"
"$BIN" --self-test >/dev/null

echo "check-v133-meta-self-benchmark: rising σ → regression_detected"
R="$("$BIN" --rising-demo)"
echo "  $R"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$R""").group(0))
assert d["n_snapshots"] == 8, d
assert d["slope_per_week"] > 0.025, f"slope {d['slope_per_week']} too small"
assert d["verdict"] == "regression_detected", d
print(f"  slope/week={d['slope_per_week']:.4f} verdict={d['verdict']} ok")
EOF

echo "check-v133-meta-self-benchmark: noisy σ → calibration_drift"
N="$("$BIN" --noisy-demo)"
echo "  $N"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$N""").group(0))
assert d["verdict"]   == "calibration_drift", d
assert d["meta_sigma"] > 0.40, f"meta-σ {d['meta_sigma']} too small"
print(f"  meta_sigma={d['meta_sigma']:.4f} verdict={d['verdict']} ok")
EOF

echo "check-v133-meta-self-benchmark: diagnose channel-6 red-team spike"
D="$("$BIN" --diagnose 0.1,0.1,0.1,0.1,0.1,0.1,0.9,0.1)"
echo "  $D"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$D""").group(0))
assert d["cause"] == "red_team_fail", d
print(f"  cause={d['cause']} ok")
EOF

echo "check-v133-meta-self-benchmark: diagnose channel-1 kv spike"
D2="$("$BIN" --diagnose 0.1,0.85,0.1,0.1,0.1,0.1,0.1,0.1)"
echo "  $D2"
python3 - <<EOF
import json, re
d = json.loads(re.search(r'\{.*\}', """$D2""").group(0))
assert d["cause"] == "kv_cache_degenerate", d
print(f"  cause={d['cause']} ok")
EOF

echo "check-v133-meta-self-benchmark: OK"
