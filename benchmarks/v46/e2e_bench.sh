#!/usr/bin/env bash
# v46 end-to-end benchmark — stub until creation_os_v46 + eval harness are wired.

set -eu

echo "# v46 e2e benchmark stub"
echo ""
echo "This repo ships fast σ-from-logits + SIMD lab code (make check-v46), not an archived"
echo "BitNet+bench JSON matrix under benchmarks/v46/."
echo "To falsify headline tables, you need:"
echo "  - pinned bitnet.cpp (or engine) build + tokenizer,"
echo "  - logged per-token logits + σ + latency + energy (instrumented),"
echo "  - and aggregate.py inputs (see docs/REPRO_BUNDLE_TEMPLATE.md)."
echo ""
echo "When wired, the intended loop is:"
echo "  ./creation_os_v46 --model bitnet-2b-sigma --eval truthfulqa_mc1 --measure-latency --measure-energy --measure-sigma --output benchmarks/v46/bitnet-2b-sigma_truthfulqa_mc1.json"
echo "  python3 benchmarks/v46/aggregate.py --inputs benchmarks/v46/*.json --output benchmarks/v46/RESULTS.md"
echo ""
echo "SKIP: set RUN_V46_E2E_HARNESS=1 after wiring creation_os_v46 CLI + datasets."

if test "${RUN_V46_E2E_HARNESS:-0}" = "1"; then
  echo "RUN_V46_E2E_HARNESS=1 set — placeholder for future harness (no-op today)."
fi

exit 0
