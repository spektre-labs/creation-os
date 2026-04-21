#!/usr/bin/env bash
# NEXT-4 smoke: cos-mcp (JSON-RPC 2.0 MCP server) — verifies that
# every cos.* tool answers through the canonical envelope.
#
# Validates:
#   * initialize advertises the MCP protocol version
#   * tools/list names all 6 cos.* tools
#   * each tool returns a result object (or a structured error)
#   * unknown-method hits -32601
set -euo pipefail
cd "$(dirname "$0")/../.."

[[ -x ./cos-mcp ]] || { echo "missing ./cos-mcp (run 'make cos-mcp')" >&2; exit 2; }

echo "  · cos-mcp --demo envelope"
DEMO="$(./cos-mcp --demo)"
grep -q '"kernel":"cos_mcp"'                  <<<"$DEMO" || { echo "no kernel tag"     >&2; exit 3; }
grep -q '"protocol_version":"2026-03-26"'     <<<"$DEMO" || { echo "no protocol ver"   >&2; exit 4; }
grep -q '"tools_offered":6'                   <<<"$DEMO" || { echo "expected 6 tools"  >&2; exit 5; }
grep -q '"name":"cos.chat"'                   <<<"$DEMO" || { echo "no cos.chat"       >&2; exit 6; }
grep -q '"name":"cos.sigma"'                  <<<"$DEMO" || { echo "no cos.sigma"      >&2; exit 7; }
grep -q '"name":"cos.calibrate"'              <<<"$DEMO" || { echo "no cos.calibrate"  >&2; exit 8; }
grep -q '"name":"cos.health"'                 <<<"$DEMO" || { echo "no cos.health"     >&2; exit 9; }
grep -q '"name":"cos.engram.lookup"'          <<<"$DEMO" || { echo "no engram.lookup"  >&2; exit 10; }
grep -q '"name":"cos.introspect"'             <<<"$DEMO" || { echo "no cos.introspect" >&2; exit 11; }
grep -q '"code":-32601'                       <<<"$DEMO" || { echo "unknown-method not -32601" >&2; exit 12; }

echo "  · cos-mcp --stdio tools/call cos.health"
H="$(printf '{"jsonrpc":"2.0","id":77,"method":"tools/call","params":{"name":"cos.health","arguments":{}}}\n' | ./cos-mcp --once)"
grep -q '"id":77'                             <<<"$H" || { echo "no id=77 echo"        >&2; exit 13; }
grep -q '"proofs_lean":"6/6"'                 <<<"$H" || { echo "no proofs_lean"       >&2; exit 14; }
grep -q '"proofs_frama_c":"15/15"'            <<<"$H" || { echo "no proofs_frama_c"    >&2; exit 15; }

echo "  · cos-mcp --stdio tools/call cos.chat"
C="$(printf '{"jsonrpc":"2.0","id":88,"method":"tools/call","params":{"name":"cos.chat","arguments":{"prompt":"What is 2+2?"}}}\n' | ./cos-mcp --once)"
grep -q '"id":88'                             <<<"$C" || { echo "no id=88 echo"        >&2; exit 16; }
grep -qE '"sigma":0\.[0-9]+'                  <<<"$C" || { echo "no sigma"              >&2; exit 17; }
grep -qE '"action":"(ACCEPT|RETHINK|ABSTAIN)"' <<<"$C" || { echo "no action"            >&2; exit 18; }

echo "  · cos-mcp --stdio tools/call cos.introspect (short-prompt bump)"
I="$(printf '{"jsonrpc":"2.0","id":99,"method":"tools/call","params":{"name":"cos.introspect","arguments":{"prompt":"why?"}}}\n' | ./cos-mcp --once)"
grep -qE '"sigma_perception":0\.3[0-9]+'      <<<"$I" || { echo "perception not bumped" >&2; exit 19; }
grep -qE '"sigma_social":0\.4[0-9]+'          <<<"$I" || { echo "social not bumped"     >&2; exit 20; }

echo "check-cos-mcp: PASS (initialize + tools/list + cos.chat/sigma/health/introspect + unknown-method -32601)"
