#!/usr/bin/env bash
# v43 σ-guided knowledge distillation — benchmark stub (honest tier-3 gate).
#
# This repo ships math + policy hooks (make check-v43), not an archived
# creation_os --distill + TruthfulQA / toxic-transfer harness.

set -eu

echo "# v43 σ-guided knowledge distillation — benchmark stub"
echo ""
echo "This repo ships scaffolding (make check-v43), not an in-tree multi-teacher KD + eval harness."
echo "To falsify headline claims (TruthfulQA, calibration drift, toxic transfer), you need:"
echo "  - frozen teacher + student weights + tokenizer,"
echo "  - a reproducible distillation driver that logs per-token teacher σ and student σ,"
echo "  - and post-hoc eval passes with archived outputs (see docs/REPRO_BUNDLE_TEMPLATE.md)."
echo ""
echo "When wired, the intended comparison loop is:"
echo "  ./creation_os --distill --method standard --teacher qwen-32b --student bitnet-2b --output benchmarks/v43/distill_standard.json"
echo "  ./creation_os --distill --method sigma_weighted --teacher qwen-32b --student bitnet-2b --output benchmarks/v43/distill_sigma_weighted.json"
echo "  ./creation_os --distill --method full_v43 --teacher qwen-32b --student bitnet-2b --output benchmarks/v43/distill_full_v43.json"
echo ""
echo "SKIP: set RUN_V43_DISTILL_HARNESS=1 after wiring an evaluator + dataset bundle."

if test "${RUN_V43_DISTILL_HARNESS:-0}" = "1"; then
  echo "RUN_V43_DISTILL_HARNESS=1 set — placeholder for future harness (no-op today)."
fi

exit 0
