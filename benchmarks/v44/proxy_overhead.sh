#!/usr/bin/env bash
# v44 σ-proxy — latency / throughput overhead benchmark stub (honest tier-3 gate).

set -eu

echo "# v44 σ-proxy — overhead benchmark stub"
echo ""
echo "This repo ships loopback HTTP + stub logits (make check-v44), not an archived SGLang/vLLM A/B harness."
echo "To falsify the headline (<5 percent per-token overhead), you need:"
echo "  - pinned engine builds + tokenizer,"
echo "  - a reproducible client load generator (OpenAI-shaped),"
echo "  - and archived JSON outputs (see docs/REPRO_BUNDLE_TEMPLATE.md)."
echo ""
echo "When wired, the intended comparison is:"
echo "  python -m sglang.bench_serving --model bitnet-2b4t --num-prompts 1000 > benchmarks/v44/direct.json"
echo "  ./creation_os_proxy --port 8080 &"
echo "  python3 benchmarks/v44/bench_openai_api.py --endpoint http://127.0.0.1:8080 --num-prompts 1000 > benchmarks/v44/proxied.json"
echo "  python3 benchmarks/v44/compare_overhead.py"
echo ""
echo "SKIP: set RUN_V44_PROXY_OVERHEAD=1 after wiring bench_openai_api.py + compare_overhead.py."

if test "${RUN_V44_PROXY_OVERHEAD:-0}" = "1"; then
  echo "RUN_V44_PROXY_OVERHEAD=1 set — placeholder for future harness (no-op today)."
fi

exit 0
