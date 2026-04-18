#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v108 σ-UI — headless render check.
#
# The v108 surface is a single static web/index.html served by the
# v106 HTTP server.  This gate validates:
#
#   1. web/index.html exists
#   2. it is valid-ish HTML5 (has <!doctype html>, <html>, <body>)
#   3. it references the eight σ-channels
#   4. it references /v1/chat/completions and /v1/sigma-profile
#   5. it contains a σ_product summary tile + abstain indicator
#   6. if ./creation_os_server is built, booting it and fetching /
#      returns 200 with our index.html contents
#
# Fails loudly on structural regressions.  Skips live-server stage
# cleanly when the binary is missing (the merge-gate entry point
# builds standalone-v106 explicitly, so skip is only for cold
# clones / hosts without a C toolchain).
set -u
set -o pipefail

cd "$(dirname "$0")/.."

HTML="web/index.html"
FAIL=0
pass() { echo "  PASS  $1"; }
fail() { echo "  FAIL  $1"; FAIL=$((FAIL+1)); }

if [ ! -f "$HTML" ]; then
    echo "check-v108: FAIL (missing $HTML)"
    exit 1
fi

B=$(cat "$HTML")

# 1. Shape checks
grep -qi '<!doctype html>' <<<"$B" && pass "has <!doctype html>" || fail "missing <!doctype html>"
grep -qi '<html'           <<<"$B" && pass "has <html> tag"      || fail "missing <html>"
grep -qi '<body>'          <<<"$B" && pass "has <body> tag"      || fail "missing <body>"
grep -qi 'σ\|sigma'        <<<"$B" && pass "references σ"        || fail "does not reference σ"

# 2. Channel labels — at least 6 of the 8 canonical names present.
HITS=0
for k in entropy mean max product tail n_eff margin stab; do
    if grep -qw "$k" <<<"$B"; then HITS=$((HITS+1)); fi
done
if [ "$HITS" -ge 6 ]; then
    pass "σ-channel labels present ($HITS/8)"
else
    fail "σ-channel labels ($HITS/8 — expected >=6)"
fi

# 3. Endpoints referenced
grep -q '/v1/chat/completions' <<<"$B" && pass "POSTs to /v1/chat/completions" || fail "no /v1/chat/completions ref"
grep -q '/v1/sigma-profile'    <<<"$B" && pass "polls /v1/sigma-profile"       || fail "no /v1/sigma-profile ref"
grep -q '/v1/models'           <<<"$B" && pass "fetches /v1/models"            || fail "no /v1/models ref"

# 4. σ_product summary and abstain indicator
grep -qi 'sigma_product\|σ_product\|sp-sigma' <<<"$B" && pass "σ_product summary present" || fail "σ_product summary missing"
grep -qi 'abstain' <<<"$B" && pass "abstain indicator present"                             || fail "abstain indicator missing"

# 4b. v114 σ-swarm panel surfaces (renderSwarm + winner + resonance)
grep -q 'renderSwarm'          <<<"$B" && pass "v114 swarm renderer present"   || fail "v114 swarm renderer missing"
grep -q 'creation_os.*swarm'   <<<"$B" && pass "v114 swarm block consumed"     || fail "v114 swarm consumer missing"
grep -qi 'resonance'           <<<"$B" && pass "v114 resonance signal present" || fail "v114 resonance signal missing"

# 5. Live server stage (optional)
BIN="${COS_V106_SERVER_BIN:-./creation_os_server}"
if [ -x "$BIN" ] && command -v curl >/dev/null 2>&1; then
    pick_free_port() {
        if command -v python3 >/dev/null 2>&1; then
            python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
        else
            echo 18700
        fi
    }
    PORT="$(pick_free_port)"
    LOG="$(mktemp -t cos_v108_XXXXXX.log)"
    "$BIN" --host 127.0.0.1 --port "$PORT" --web-root "$(pwd)/web" >"$LOG" 2>&1 &
    PID=$!
    cleanup() {
        if kill -0 "$PID" 2>/dev/null; then kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; fi
        rm -f "$LOG"
    }
    trap cleanup EXIT
    for _ in 1 2 3 4 5 6 7 8 9 10; do
        curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1 && break
        sleep 0.2
    done
    CODE=$(curl -s -o /tmp/cos_v108_idx.$$ -w '%{http_code}' "http://127.0.0.1:$PORT/")
    BODY=$(cat /tmp/cos_v108_idx.$$ 2>/dev/null || true); rm -f /tmp/cos_v108_idx.$$
    if [ "$CODE" = "200" ] && grep -qi 'creation os' <<<"$BODY"; then
        pass "GET / returns 200 and serves index.html"
    else
        fail "GET / (code=$CODE len=${#BODY})"
    fi
else
    echo "  SKIP  live-server stage (no $BIN or curl)"
fi

if [ "$FAIL" -gt 0 ]; then
    echo "check-v108: $FAIL failure(s)"
    exit 1
fi
echo "check-v108: OK (σ-UI static + optional live render)"
exit 0
