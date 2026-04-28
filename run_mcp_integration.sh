#!/usr/bin/env bash
# MCP σ-gate integration smoke: install MCP SDK in LSD venv, import server module.
# Optional commit: CREATION_MCP_INTEGRATION_COMMIT=1 bash run_mcp_integration.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"
# shellcheck source=/dev/null
source benchmarks/sigma_gate_lsd/.venv/bin/activate
export PYTHONPATH="${ROOT}/python"

LOG_DIR="${ROOT}/benchmarks/sigma_gate_eval/logs"
mkdir -p "${LOG_DIR}"
LOG="${LOG_DIR}/mcp_integration_$(date +%Y%m%d_%H%M%S).log"

echo "=== MCP σ-gate integration: $(date) ===" | tee "${LOG}"

pip install 'mcp[cli]' --quiet 2>&1 | tee -a "${LOG}"

python3 -c "from mcp.server.fastmcp import FastMCP; import cos.mcp_sigma_server; print('MCP server module: OK')" 2>&1 | tee -a "${LOG}"

if [[ "${CREATION_MCP_INTEGRATION_COMMIT:-0}" == "1" ]]; then
  git add python/cos/mcp_sigma_server.py python/cos/mcp_sigma_audit.py scripts/cos_mcp_server.py \
    configs/mcp/bifrost_sigma_gate.example.yaml docs/MCP_SIGMA.md README.md Makefile run_mcp_integration.sh 2>/dev/null || true
  git commit -m "σ-gate MCP server: stdio tools + audit ring $(date +%Y-%m-%d)" || true
fi

echo "=== COMPLETE: $(date) ===" | tee -a "${LOG}"
