#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Headless MCP stdio protocol smoke (tools/mcp/sigma_server.py). No inference backend required.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
PY="${PYTHON:-python3}"
MCP="$ROOT/tools/mcp/sigma_server.py"
echo "=== MCP stdio E2E (sigma_server.py) ==="

tmp="$(mktemp)"
cleanup() { rm -f "$tmp" /tmp/mcp_init.json /tmp/mcp_tools.json /tmp/mcp_call.json 2>/dev/null || true; }
trap cleanup EXIT

echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","capabilities":{},"clientInfo":{"name":"test","version":"1.0"}}}' \
  | "$PY" "$MCP" > /tmp/mcp_init.json
"$PY" - <<'PY'
import json
r = json.load(open("/tmp/mcp_init.json"))
assert r.get("result", {}).get("protocolVersion") == "2024-11-05", r
print("PASS: initialize")
PY

echo '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
  | "$PY" "$MCP" > /tmp/mcp_tools.json
"$PY" - <<'PY'
import json
r = json.load(open("/tmp/mcp_tools.json"))
tools = [t["name"] for t in r["result"]["tools"]]
assert "sigma_gate" in tools, tools
assert "sigma_verify" in tools, tools
print("PASS: tools/list")
PY

echo '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"sigma_gate","arguments":{"text":"protocol-smoke"}}}' \
  | "$PY" "$MCP" > /tmp/mcp_call.json
"$PY" - <<'PY'
import json
r = json.load(open("/tmp/mcp_call.json"))
assert "result" in r, r
body = r["result"]
assert "content" in body and body["content"], body
assert "isError" in body
print("PASS: tools/call (MCP content envelope)")
PY

echo "=== All MCP stdio tests passed ==="
