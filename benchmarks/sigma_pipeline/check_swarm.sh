#!/usr/bin/env bash
# HORIZON-4: σ-swarm coordinator + stigmergy (HTTP peers; σ-ranked merge).
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos ]] || { echo "missing ./cos" >&2; exit 2; }
make -s cos-swarm >/dev/null
[[ -x ./cos-swarm ]] || { echo "missing ./cos-swarm" >&2; exit 2; }

echo "  · cos-swarm --self-test (legacy argv → cos swarm)"
./cos-swarm --self-test

echo "  · cos swarm --self-test"
./cos swarm --self-test

echo "  · cos swarm peers"
./cos swarm peers

echo "check-swarm: PASS (self-test + cos swarm forward)"
