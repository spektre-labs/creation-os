#!/usr/bin/env bash
# HORIZON-4: σ-swarm routing (argmin σ + trust EMA; mock peers in v0).
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-swarm ]] || { echo "missing ./cos-swarm" >&2; exit 2; }
[[ -x ./cos ]] || { echo "missing ./cos" >&2; exit 2; }

echo "  · cos-swarm --self-test"
./cos-swarm --self-test

echo "  · cos-swarm --once (lowest-σ peer is p1 → localhost:8082)"
out="$(./cos-swarm --peers 8081,8082,8083 --once --prompt "What is 2+2?")" || true
grep -q 'routing → p1' <<<"$out" || { echo "$out" >&2; exit 3; }
grep -q 'SWARM:8082' <<<"$out" || { echo "$out" >&2; exit 4; }

echo "  · cos swarm --self-test (dispatcher)"
./cos swarm --self-test

echo "check-swarm: PASS (self-test + once + cos forward)"
