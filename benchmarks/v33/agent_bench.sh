#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p metrics "benchmarks/v33/metrics_run"
export COS_V33_REPO_ROOT="$ROOT"
export COS_V33_METRICS_DIR="$ROOT/benchmarks/v33/metrics_run"
make standalone-v33
OUT="$ROOT/benchmarks/v33/AGENT_RESULTS.md"
{
  echo "# v33 agent harness (synthetic 50-step router + schema loop)"
  echo
  echo "Host: $(uname -a 2>/dev/null || true)"
  echo "Date: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo
  echo "This harness exercises \`creation_os_v33 --bench-agent\` with three routing policies."
  echo "CPS (€/successful task) is not computed when all models are local cost=0; see SUMMARY lines for rates."
  echo
} >"$OUT"
for mode in bitnet-only "bitnet+fallback" fallback-only; do
  echo "## Mode: ${mode}" >>"$OUT"
  echo '```' >>"$OUT"
  COS_V33_BENCH_MODE="$mode" ./creation_os_v33 --bench-agent | tee -a "$OUT"
  echo '```' >>"$OUT"
  echo >>"$OUT"
done
echo "Wrote $OUT"
