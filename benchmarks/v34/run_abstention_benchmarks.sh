#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
make standalone-v34
echo "# Abstention / UQ benchmark harness (v34)"
echo
echo "TruthfulQA, FreshQA, and SelfAware **bundles are not vendored in-tree**."
echo "Until weights + datasets + harness land in an archived repro bundle, this script only runs **local smoke** metrics."
echo
./creation_os_v34 --bench-synthetic-auroc
echo
echo "When datasets are present, extend this script to run:"
echo "  1) BitNet baseline (no σ gate)"
echo "  2) BitNet + σ_total threshold (legacy router)"
echo "  3) BitNet + σ_epistemic / Platt router (v34)"
echo "and report AUROC, ECE, abstention rate, accuracy-on-answered."
