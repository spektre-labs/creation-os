#!/usr/bin/env bash
# v45 “Gemini Paradox” / introspection scatter — benchmark stub (honest tier-3 gate).

set -eu

echo "# v45 introspection / paradox benchmark stub"
echo ""
echo "This repo ships deterministic σ-introspection math (make check-v45), not an archived"
echo "TruthfulQA MC1 + per-model self-report harness with public scatter JSON."
echo "To falsify headline scatter claims, you need:"
echo "  - frozen model endpoints + tokenizer,"
echo "  - logged (accuracy, calibration_gap) pairs per model,"
echo "  - and archived outputs (see docs/REPRO_BUNDLE_TEMPLATE.md)."
echo ""
echo "When wired, the intended loop is:"
echo "  ./creation_os --eval truthfulqa_mc1 --introspection-test --model bitnet-2b --output benchmarks/v45/introspection_bitnet-2b.json"
echo "  ./creation_os --eval truthfulqa_mc1 --introspection-test --model claude-api --output benchmarks/v45/introspection_claude-api.json"
echo "  ./creation_os --eval truthfulqa_mc1 --introspection-test --model gemini-api --output benchmarks/v45/introspection_gemini-api.json"
echo "  python3 benchmarks/v45/plot_paradox.py"
echo ""
echo "SKIP: set RUN_V45_PARADOX_HARNESS=1 after wiring creation_os eval + plot inputs."

if test "${RUN_V45_PARADOX_HARNESS:-0}" = "1"; then
  echo "RUN_V45_PARADOX_HARNESS=1 set — placeholder for future harness (no-op today)."
fi

exit 0
