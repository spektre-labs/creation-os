#!/usr/bin/env bash
# check-v136: σ-evolve — ES architecture search convergence
#
# Verifies:
#   1. self-test passes (ES beats uniform baseline by ≥0.02 and hits ≥0.80)
#   2. demo run writes a TOML with [sigma_aggregator] section
#   3. trajectory is monotone non-decreasing (elitism invariant)
#   4. margin = evolved_fitness - baseline_fitness > 0
set -euo pipefail
BIN="./creation_os_v136_evolve"
[ -x "$BIN" ] || { echo "check-v136: $BIN not built"; exit 1; }

echo "check-v136: self-test"
"$BIN" --self-test

echo "check-v136: demo run + TOML output"
tmp=$(mktemp -t v136_XXXX.toml)
out=$("$BIN" --demo --seed 42 --out "$tmp")
echo "$out" | head -3

margin=$(printf '%s\n' "$out" \
         | awk -F'[= ]' '/^baseline_fitness/{for(i=1;i<=NF;i++) if($i=="margin") print $(i+1)}')
awk -v m="$margin" 'BEGIN{ if (m+0 <= 0.0) { print "no improvement over baseline: "m; exit 1 } }'
echo "  margin=$margin > 0 OK"

grep -q '\[sigma_aggregator\]' "$tmp" || { echo "missing [sigma_aggregator]"; exit 1; }
grep -q 'kind = "weighted_geometric"' "$tmp" || { echo "wrong kind"; exit 1; }
grep -q 'weights = \[' "$tmp" || { echo "no weights array"; exit 1; }
echo "  TOML shape OK"

traj=$(printf '%s\n' "$out" \
       | awk -F= '/^trajectory=/{print $2}')
python3 - <<PY
traj = [float(x) for x in """$traj""".strip().split(",") if x]
assert len(traj) >= 5, traj
for i in range(1, len(traj)):
    assert traj[i] + 1e-6 >= traj[i-1], f"non-monotone at {i}: {traj[i-1]} -> {traj[i]}"
print("  trajectory monotone (n=%d first=%.4f last=%.4f)" % (len(traj), traj[0], traj[-1]))
PY

rm -f "$tmp"
echo "check-v136: all OK"
