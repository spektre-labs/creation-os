#!/usr/bin/env bash
# v116 σ-MCP smoke: exercises the stdio JSON-RPC 2.0 server end-to-end.
#
# Verifies:
#   1. self-test passes (pure-C dispatcher)
#   2. initialize handshake returns serverInfo + sigmaGovernance capability
#   3. tools/list returns all 5 advertised Creation OS tools
#   4. resources/list returns 3 cos:// resources
#   5. prompts/list returns cos_analyze and cos_verify
#   6. prompts/get with arguments substitutes the template hole
#   7. tools/call cos_sigma_profile returns an 8-channel σ schema
#   8. tools/call cos_memory_search with no store returns a structured
#      "unavailable" error (so the host knows the capability is present
#      but not wired)

set -euo pipefail

BIN=${BIN:-./creation_os_v116_mcp}
if [ ! -x "$BIN" ]; then
    echo "[v116] $BIN missing — run 'make creation_os_v116_mcp' first" >&2
    exit 1
fi

echo "[v116] self-test"
"$BIN" --self-test

dispatch() {
    "$BIN" --dispatch "$1"
}

echo "[v116] initialize"
INIT=$(dispatch '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18"}}')
for needle in '"protocolVersion":"2025-06-18"' 'creation-os' 'sigmaGovernance' 'sigma_product'; do
    if ! echo "$INIT" | grep -q "$needle"; then
        echo "[v116] FAIL: initialize missing $needle" >&2
        echo "  body: $INIT" >&2
        exit 1
    fi
done

echo "[v116] tools/list"
TL=$(dispatch '{"jsonrpc":"2.0","id":2,"method":"tools/list"}')
for t in cos_chat cos_reason cos_memory_search cos_sandbox_execute cos_sigma_profile; do
    if ! echo "$TL" | grep -q "\"$t\""; then
        echo "[v116] FAIL: tools/list missing $t" >&2
        exit 1
    fi
done

echo "[v116] resources/list"
RL=$(dispatch '{"jsonrpc":"2.0","id":3,"method":"resources/list"}')
for r in "cos://memory/" "cos://knowledge/" "cos://sigma/history"; do
    if ! echo "$RL" | grep -q "$r"; then
        echo "[v116] FAIL: resources/list missing $r" >&2
        exit 1
    fi
done

echo "[v116] prompts/list + get"
PL=$(dispatch '{"jsonrpc":"2.0","id":4,"method":"prompts/list"}')
echo "$PL" | grep -q 'cos_analyze' || { echo "[v116] FAIL: prompts missing cos_analyze"; exit 1; }
echo "$PL" | grep -q 'cos_verify'  || { echo "[v116] FAIL: prompts missing cos_verify";  exit 1; }

PG=$(dispatch '{"jsonrpc":"2.0","id":5,"method":"prompts/get","params":{"name":"cos_analyze","arguments":{"content":"SMOKE_TOKEN"}}}')
echo "$PG" | grep -q 'SMOKE_TOKEN' \
    || { echo "[v116] FAIL: prompt template did not substitute content"; exit 1; }

echo "[v116] tools/call cos_sigma_profile"
TC=$(dispatch '{"jsonrpc":"2.0","id":6,"method":"tools/call","params":{"name":"cos_sigma_profile","arguments":{}}}')
for ch in sigma_product sigma_mean sigma_max_token entropy sigma_tail_mass sigma_n_effective margin stability; do
    if ! echo "$TC" | grep -q "\"$ch\""; then
        echo "[v116] FAIL: sigma_profile missing $ch" >&2
        exit 1
    fi
done

echo "[v116] tools/call cos_memory_search (expected unavailable)"
TC2=$(dispatch '{"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":"cos_memory_search","arguments":{"query":"paris"}}}')
echo "$TC2" | grep -q '"error"'      || { echo "[v116] FAIL: expected error for unwired memory_search"; exit 1; }
echo "$TC2" | grep -q 'unavailable'  || { echo "[v116] FAIL: expected unavailable marker"; exit 1; }
echo "$TC2" | grep -q -- '-32000'   || { echo "[v116] FAIL: expected code -32000"; exit 1; }

echo "[v116] stdio loop (1 request)"
printf '%s\n' '{"jsonrpc":"2.0","id":42,"method":"ping"}' \
    | "$BIN" --stdio | grep -q '"id":42' \
    || { echo "[v116] FAIL: stdio ping did not return id=42"; exit 1; }

echo "[v116] OK"
