#!/usr/bin/env bash
# HORIZON-5: cos-sandbox front door (σ-Sandbox exec wrapper).
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-sandbox ]] || { echo "missing ./cos-sandbox" >&2; exit 2; }
[[ -x ./cos ]] || { echo "missing ./cos" >&2; exit 2; }

echo "  · cos-sandbox --self-test"
./cos-sandbox --self-test

echo "  · cos-sandbox --risk 0 -- /bin/echo H5_OK"
out="$(./cos-sandbox --risk 0 -- /bin/echo H5_OK)" || true
grep -q 'H5_OK' <<<"$out" || { echo "$out" >&2; exit 3; }
grep -q 'rc=0' <<<"$out" || { echo "$out" >&2; exit 4; }

echo "  · cos sandbox --self-test (dispatcher)"
./cos sandbox --self-test

echo "check-sandbox: PASS (self-test + risk0 + cos forward)"
