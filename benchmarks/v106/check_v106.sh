#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v106 σ-Server — loopback curl smoke.
#
# Starts `./creation_os_server` (stub mode — no GGUF needed) on a
# random free port, then exercises:
#   GET  /health                 expects 200 "ok"
#   GET  /v1/models              expects 200 JSON with .data[0].id
#   GET  /v1/sigma-profile       expects 200 JSON with has_data:false (no calls yet)
#   POST /v1/chat/completions    expects 503 (no model loaded, stub mode)
#   OPTIONS /v1/chat/completions expects 204 with CORS headers
#
# Merge-gate-safe: every check has a pass/fail print, and the
# script exits non-zero on any failure.  If curl is missing on the
# host, the script prints SKIP and exits 0 (the C self-test has
# already run in check-v106).
#
# Env knobs:
#   COS_V106_SERVER_BIN   path to binary (default ./creation_os_server)
#   COS_V106_SERVER_PORT  port to try first (default 0 → kernel assigns)
#   COS_V106_KEEP_LOG     keep server log file on exit (default unset)
set -u
set -o pipefail

cd "$(dirname "$0")/../.."

BIN="${COS_V106_SERVER_BIN:-./creation_os_server}"
if [ ! -x "$BIN" ]; then
    echo "check-v106 smoke: SKIP (binary $BIN missing — run 'make standalone-v106' first)"
    exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "check-v106 smoke: SKIP (curl not installed)"
    exit 0
fi

# Pick a free port with python/perl if available; else hardcode a
# range.  We don't want a clash with a user's existing 8080 server.
pick_free_port() {
    if command -v python3 >/dev/null 2>&1; then
        python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
    elif command -v python >/dev/null 2>&1; then
        python  -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
    else
        # Crude fallback: try ports in [18700..18800), pick the first
        # `nc -z` says is free.
        for p in $(seq 18700 18800); do
            if ! (echo > "/dev/tcp/127.0.0.1/$p") 2>/dev/null; then
                echo "$p"
                return
            fi
        done
        echo "8080"
    fi
}

PORT="${COS_V106_SERVER_PORT:-$(pick_free_port)}"
LOG="$(mktemp -t cos_v106_XXXXXX.log)"

"$BIN" --host 127.0.0.1 --port "$PORT" \
       --web-root "$(pwd)/web" \
       > "$LOG" 2>&1 &
PID=$!

cleanup() {
    if kill -0 "$PID" 2>/dev/null; then
        kill "$PID" 2>/dev/null || true
        # Wait briefly and then silence the "Terminated: 15" message
        # that bash prints when the backgrounded process is reaped.
        wait "$PID" 2>/dev/null || true
    fi
    if [ -z "${COS_V106_KEEP_LOG:-}" ]; then
        rm -f "$LOG"
    else
        echo "check-v106 smoke: server log kept at $LOG"
    fi
}
trap cleanup EXIT

# Wait for the server to bind (at most ~2s).
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done

if ! curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then
    echo "check-v106 smoke: FAIL (server never bound on port $PORT)"
    cat "$LOG"
    exit 1
fi

FAIL=0
pass() { echo "  PASS  $1"; }
fail() { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

# 1. /health → 200 "ok"
RESP=$(curl -fsS "http://127.0.0.1:$PORT/health")
if [ "$RESP" = "ok" ] || [ "$RESP" = "ok\n" ] || [[ "$RESP" == ok* ]]; then
    pass "/health returns ok"
else
    fail "/health returns ok (got: $RESP)"
fi

# 2. /v1/models → 200 JSON mentioning the default model id.
RESP=$(curl -fsS "http://127.0.0.1:$PORT/v1/models")
if echo "$RESP" | grep -q '"object":"list"' && echo "$RESP" | grep -q '"object":"model"'; then
    pass "/v1/models returns OpenAI list shape"
else
    fail "/v1/models returns list (got: $RESP)"
fi
if echo "$RESP" | grep -q '"sigma_aggregator":"product"'; then
    pass "/v1/models reports v105 default aggregator (product)"
else
    fail "/v1/models aggregator hint (got: $RESP)"
fi

# 3. /v1/sigma-profile → 200 JSON with has_data:false before any call.
RESP=$(curl -fsS "http://127.0.0.1:$PORT/v1/sigma-profile")
if echo "$RESP" | grep -q '"has_data":false'; then
    pass "/v1/sigma-profile reports has_data:false when no inference has run"
else
    fail "/v1/sigma-profile has_data:false (got: $RESP)"
fi
if echo "$RESP" | grep -q '"sigma_profile":\['; then
    pass "/v1/sigma-profile contains the eight-channel profile array"
else
    fail "/v1/sigma-profile channel array (got: $RESP)"
fi

# 4. POST /v1/chat/completions → 503 in stub mode (no GGUF loaded).
CODE=$(curl -s -o /tmp/cos_v106_resp.$$ -w '%{http_code}' \
       -H 'Content-Type: application/json' \
       -d '{"model":"test","messages":[{"role":"user","content":"hi"}]}' \
       "http://127.0.0.1:$PORT/v1/chat/completions")
BODY=$(cat /tmp/cos_v106_resp.$$ 2>/dev/null || true)
rm -f /tmp/cos_v106_resp.$$
if [ "$CODE" = "503" ] && echo "$BODY" | grep -q 'not_loaded'; then
    pass "/v1/chat/completions returns 503 not_loaded in stub mode"
else
    fail "/v1/chat/completions stub-mode 503 (code=$CODE body=$BODY)"
fi

# 5. OPTIONS preflight → 204 with CORS headers.
CODE=$(curl -s -o /dev/null -w '%{http_code}' \
       -X OPTIONS -H 'Origin: http://example' \
       "http://127.0.0.1:$PORT/v1/chat/completions")
if [ "$CODE" = "204" ]; then
    pass "OPTIONS /v1/chat/completions returns 204 for CORS preflight"
else
    fail "OPTIONS /v1/chat/completions 204 (code=$CODE)"
fi

# 6. Unknown method → 405
CODE=$(curl -s -o /dev/null -w '%{http_code}' -X PUT \
       "http://127.0.0.1:$PORT/v1/chat/completions")
if [ "$CODE" = "405" ]; then
    pass "unknown HTTP method returns 405"
else
    fail "unknown method 405 (code=$CODE)"
fi

# 7. Missing user message → 400 invalid_request
CODE=$(curl -s -o /tmp/cos_v106_bad.$$ -w '%{http_code}' \
       -H 'Content-Type: application/json' \
       -d '{"model":"t","messages":[{"role":"system","content":"x"}]}' \
       "http://127.0.0.1:$PORT/v1/chat/completions")
BODY=$(cat /tmp/cos_v106_bad.$$ 2>/dev/null || true)
rm -f /tmp/cos_v106_bad.$$
if [ "$CODE" = "400" ] && echo "$BODY" | grep -q 'invalid_request'; then
    pass "no user message → 400 invalid_request"
else
    fail "no user message → 400 (code=$CODE body=$BODY)"
fi

if [ "$FAIL" -gt 0 ]; then
    echo "check-v106 smoke: $FAIL failure(s) — server log:"
    echo "-- BEGIN server log --"
    cat "$LOG"
    echo "-- END server log --"
    exit 1
fi

echo "check-v106 smoke: OK (7 checks PASS — loopback curl exercised the v106 HTTP surface)"
exit 0
