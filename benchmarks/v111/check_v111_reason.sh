#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v111.2 σ-Reason loopback check — merge-gate safe.
#
# Starts the stub-mode v106 server and probes /v1/reason.  In stub mode
# the bridge is not loaded, so the endpoint MUST return 503
# `not_loaded` with a structured error object (the same contract as
# /v1/chat/completions in stub mode).  When a real GGUF is available
# the same endpoint is expected to return a 200 with a `reasoning`
# object; that path is exercised in bench-v111-hellaswag and
# bench-v111-full and is NOT required for the gate.
set -eo pipefail
cd "$(dirname "$0")/../.."

SERVER=./creation_os_server
if [ ! -x "$SERVER" ]; then
    echo "check-v111-reason: SKIP (no $SERVER — run 'make standalone-v106' first)"
    exit 0
fi

PORT="${COS_V111_REASON_PORT:-17111}"
TMPDIR=$(mktemp -d)
CFG="$TMPDIR/config.toml"
cat > "$CFG" <<EOF
[server]
host = "127.0.0.1"
port = $PORT

[sigma]
tau_abstain = 0.70
aggregator  = "product"

[model]
gguf_path = ""
id        = "v111-test"
EOF

$SERVER --config "$CFG" > "$TMPDIR/server.log" 2>&1 &
PID=$!
cleanup() {
    kill "$PID" 2>/dev/null || true
    wait "$PID" 2>/dev/null || true
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

# Wait up to 5s for the port
for i in $(seq 1 50); do
    if curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then break; fi
    sleep 0.1
done
if ! curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then
    echo "check-v111-reason: FAIL — server did not start"
    cat "$TMPDIR/server.log" >&2
    exit 1
fi

PASS=0; FAIL=0
expect () {
    # $1 description  $2 curl args  $3 expected status  $4 expected substring
    local desc="$1"; shift
    local expected_status="$1"; shift
    local expected_substr="$1"; shift
    local body status
    local tmp="$TMPDIR/resp"
    status=$(curl -sS -o "$tmp" -w "%{http_code}" "$@") || status="ERR"
    body=$(cat "$tmp" 2>/dev/null || true)
    if [ "$status" = "$expected_status" ] && echo "$body" | grep -q "$expected_substr"; then
        PASS=$((PASS+1))
        echo "  [ok] $desc ($status)"
    else
        FAIL=$((FAIL+1))
        echo "  [FAIL] $desc — got status=$status body=$body"
    fi
}

BASE="http://127.0.0.1:$PORT"

# 1. /v1/reason with prompt key → 503 not_loaded (stub mode)
expect "POST /v1/reason prompt=stub → 503" 503 'not_loaded' \
    -X POST "$BASE/v1/reason" \
    -H 'Content-Type: application/json' \
    -d '{"prompt":"test","k_candidates":2,"max_tokens":4}'

# 2. /v1/reason with messages → same 503 (falls back to last-user extraction)
expect "POST /v1/reason messages=stub → 503" 503 'not_loaded' \
    -X POST "$BASE/v1/reason" \
    -H 'Content-Type: application/json' \
    -d '{"messages":[{"role":"user","content":"q?"}],"k_candidates":1}'

# 3. /v1/reason missing both → 400 invalid_request
expect "POST /v1/reason malformed → 400" 400 'missing prompt' \
    -X POST "$BASE/v1/reason" \
    -H 'Content-Type: application/json' \
    -d '{}'

# 4. CORS preflight
expect "OPTIONS /v1/reason → 204" 204 '' \
    -X OPTIONS "$BASE/v1/reason"

# 5. GET /v1/reason → 404 (server registers no GET handler for this path;
# consistent with /v1/chat/completions GET behaviour in v106).
expect "GET /v1/reason → 404" 404 'not found' \
    -X GET "$BASE/v1/reason"

echo "check-v111-reason: $PASS PASS / $FAIL FAIL"
[ "$FAIL" = "0" ]
