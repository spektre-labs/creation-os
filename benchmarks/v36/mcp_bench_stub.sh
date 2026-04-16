#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
make standalone-mcp
echo "# MCP σ server benchmark stub (v36)"
echo
echo "Real Claude Desktop A/B (TruthfulQA / GSM8K) is **not vendored** in-tree."
echo "Use this stub to sanity-check JSON-RPC locally:"
echo
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' | ./creation_os_mcp --transport stdio | head -c 200 && echo
printf '%s\n' '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"should_abstain","arguments":{"logits":[0,0,0,0,0,0,0,0],"threshold_profile":"moderate"}}}' | ./creation_os_mcp --transport stdio
echo
echo "Planned metrics when harness exists: accuracy-on-answered, abstention rate, latency overhead."
