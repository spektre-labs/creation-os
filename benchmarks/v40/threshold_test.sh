#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v40 σ-threshold harness stub — real TruthfulQA sweeps require weights + eval CLI (not vendored here).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "# v40 σ-threshold benchmark stub"
echo
echo "Goal (hypothesis): measure whether hallucination metrics improve sharply as independent σ-channels increase."
echo "This repo does not ship a creation_os --eval truthfulqa_mc1 entrypoint today."
echo
echo "When a harness exists, the intended loop is:"
echo "  for n in 1 2 3 4 5 6 7 8; do"
echo "    SIGMA_CHANNELS=\$n <your_evaluator> > benchmarks/v40/truthfulqa_\${n}ch.json"
echo "  done"
echo "  python3 benchmarks/v40/plot_threshold.py"
echo
if [[ "${RUN_V40_THRESHOLD_HARNESS:-0}" == "1" ]]; then
  echo "RUN_V40_THRESHOLD_HARNESS=1 set, but no evaluator is wired yet — failing loudly."
  exit 2
fi

echo "SKIP: set RUN_V40_THRESHOLD_HARNESS=1 after wiring an evaluator + dataset bundle (see docs/REPRO_BUNDLE_TEMPLATE.md)."
exit 0
