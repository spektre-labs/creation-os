#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
make standalone-v35
echo "# σ-guided speculative decoding — benchmark harness (v35)"
echo
echo "Full BitNet+Qwen (or API verifier) throughput and TruthfulQA rows are **not vendored** here."
echo "This script runs **synthetic** smoke lines via \`creation_os_v35 --bench-spec-synthetic\`."
echo
./creation_os_v35 --bench-spec-synthetic
echo
echo "Planned modes (real harness):"
echo "  1) BitNet only, no spec decode"
echo "  2) Spec decode: BitNet draft + verifier, fixed K=4"
echo "  3) σ-guided spec: adaptive K from epistemic unit score"
echo "  4) σ-guided + dual abstain when verifier epistemic is high"
