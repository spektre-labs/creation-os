#!/usr/bin/env bash
# HORIZON-3: long-horizon plan + σ-checkpoints.
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-plan ]] || { echo "missing ./cos-plan" >&2; exit 2; }
[[ -x ./cos ]] || { echo "missing ./cos" >&2; exit 2; }

echo "  · cos-plan --self-test"
./cos-plan --self-test

echo "  · cos-plan --demo (snapshots under /tmp)"
root="/tmp/cos_lh_harness_$$"
rm -rf "$root"
out="$(COS_PLAN_SNAPSHOT_ROOT="$root" ./cos-plan --demo)" || true
grep -q 'Step 4/4' <<<"$out" || { echo "$out" >&2; exit 3; }
grep -q 'σ_plan=0.22' <<<"$out" || { echo "$out" >&2; exit 4; }
grep -q '4/4 complete' <<<"$out" || { echo "$out" >&2; exit 5; }
[[ -f "$root/snap_4/MANIFEST.txt" ]] || { echo "missing snap_4" >&2; exit 6; }
rm -rf "$root"

echo "  · cos-plan --rollback-demo"
out2="$(./cos-plan --rollback-demo)" || true
grep -q 'ABSTAIN' <<<"$out2" || { echo "$out2" >&2; exit 7; }
grep -q 'Rolling back to snapshot:2' <<<"$out2" || { echo "$out2" >&2; exit 8; }

echo "  · cos plan --self-test (dispatcher)"
./cos plan --self-test

echo "check-long-horizon: PASS (self-test + demo + rollback + cos forward)"
