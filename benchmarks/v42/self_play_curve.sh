#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v42 self-play improvement curve stub — requires an archived self-play + eval harness (not vendored here).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

echo "# v42 σ-guided self-play — benchmark stub"
echo
echo "This repo ships scaffolding (make check-v42), not an archived creation_os --self-play + GSM8K harness."
echo "To falsify the headline curve, you need:"
echo "  - a frozen model + tokenizer,"
echo "  - a reproducible self-play driver that logs JSONL experiences,"
echo "  - and a post-hoc eval pass with archived outputs (see docs/REPRO_BUNDLE_TEMPLATE.md)."
echo
echo "When wired, the intended loop is:"
echo "  ./creation_os --eval gsm8k,math500 --output benchmarks/v42/baseline.json"
echo "  ./creation_os --self-play --iterations 1000 --log benchmarks/v42/experience.jsonl"
echo "  ./creation_os --eval gsm8k,math500 --output benchmarks/v42/after_selfplay.json"
echo "  python3 benchmarks/v42/plot_improvement.py"
echo
if [[ "${RUN_V42_SELFPLAY_HARNESS:-0}" == "1" ]]; then
  echo "RUN_V42_SELFPLAY_HARNESS=1 set, but no evaluator is wired yet — failing loudly."
  exit 2
fi

echo "SKIP: set RUN_V42_SELFPLAY_HARNESS=1 after wiring an evaluator + dataset bundle."
exit 0
