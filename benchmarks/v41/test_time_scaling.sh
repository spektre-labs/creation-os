#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v41 test-time scaling harness stub — GSM8K / MATH / AIME require weights + eval CLI (not vendored here).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "# v41 σ-guided test-time compute — benchmark stub"
echo
echo "This repo ships **policy + receipts** in C (make check-v41), not an archived creation_os --eval harness."
echo "External literature (s1-style budget forcing, o1/o3-class depth scaling) is **not reproduced** in-tree until:"
echo "  - a frozen model artifact + tokenizer,"
echo "  - a reproducible eval driver,"
echo "  - and a REPRO bundle per docs/REPRO_BUNDLE_TEMPLATE.md."
echo
echo "When wired, the intended loop is:"
echo "  for b in 1 2 4 8 16 32 64; do"
echo "    THINK_BUDGET=\$b <your_evaluator> --suite gsm8k,math500,aime24 > benchmarks/v41/budget_\${b}.json"
echo "  done"
echo "  python3 benchmarks/v41/plot_scaling.py"
echo
if [[ "${RUN_V41_SCALING_HARNESS:-0}" == "1" ]]; then
  echo "RUN_V41_SCALING_HARNESS=1 set, but no evaluator is wired yet — failing loudly."
  exit 2
fi

echo "SKIP: set RUN_V41_SCALING_HARNESS=1 after wiring an evaluator + dataset bundle."
exit 0
