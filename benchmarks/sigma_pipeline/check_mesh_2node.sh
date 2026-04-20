#!/usr/bin/env bash
#
# FIX-6: real 2-node TCP mesh integration test.
#
# Spawns two cos-mesh-node processes on loopback ports 7001 and 7002,
# exchanges σ-Protocol frames (HEARTBEAT + QUERY→RESPONSE), and
# asserts the expected events appear in each node's JSON event log.
#
# If you can see the following four lines in the output, real TCP
# σ-Protocol traffic works:
#
#     node_A | listen_ok
#     node_B | connect → send_ok QUERY
#     node_A | recv_ok QUERY + send_response RESPONSE
#     node_B | recv_response RESPONSE
#
# The same code path runs over WAN between production peers.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./cos-mesh-node
[[ -x "$BIN" ]] || { echo "missing $BIN — run 'make cos-mesh-node'" >&2; exit 2; }

# Pick ports in a range unlikely to collide with dev services.
PORT_A=${COS_MESH_PORT_A:-37017}
PORT_B=${COS_MESH_PORT_B:-37019}

LOG_A=$(mktemp)
LOG_B=$(mktemp)
trap 'rm -f "$LOG_A" "$LOG_B"; kill 0 2>/dev/null || true' EXIT

# -- launch node A (listener only), then node B (listener + outbound
#    QUERY to A).  Each has a 1500 ms duration — the whole test takes
#    ~1.6s wall.
"$BIN" --role node_A --port "$PORT_A" --duration-ms 1500 \
    >"$LOG_A" 2>&1 &
PID_A=$!

# Give A a moment to bind before B tries to connect.
sleep 0.2

"$BIN" --role node_B --port "$PORT_B" \
       --peer "127.0.0.1:$PORT_A" \
       --send QUERY --payload "hello from B" \
       --duration-ms 1500 \
    >"$LOG_B" 2>&1 &
PID_B=$!

wait "$PID_A" "$PID_B"

echo "=== node A (port $PORT_A) log ==="
cat "$LOG_A"
echo "=== node B (port $PORT_B) log ==="
cat "$LOG_B"

# ---- assertions ----------------------------------------------------
grep -q "\"role\":\"node_A\".*\"event\":\"listen_ok\""       "$LOG_A" \
    || { echo "FAIL node_A did not report listen_ok" >&2; exit 3; }
grep -q "\"role\":\"node_A\".*\"event\":\"recv_ok\".*QUERY"  "$LOG_A" \
    || { echo "FAIL node_A did not recv QUERY from node_B" >&2; exit 4; }
grep -q "\"role\":\"node_A\".*\"event\":\"send_response\""   "$LOG_A" \
    || { echo "FAIL node_A did not send RESPONSE" >&2; exit 5; }

grep -q "\"role\":\"node_B\".*\"event\":\"listen_ok\""        "$LOG_B" \
    || { echo "FAIL node_B did not report listen_ok" >&2; exit 6; }
grep -q "\"role\":\"node_B\".*\"event\":\"send_ok\".*QUERY"   "$LOG_B" \
    || { echo "FAIL node_B did not send QUERY" >&2; exit 7; }
grep -q "\"role\":\"node_B\".*\"event\":\"recv_response\".*RESPONSE" "$LOG_B" \
    || { echo "FAIL node_B did not recv RESPONSE" >&2; exit 8; }

# Exit receipts — A should report received=1, B received=0 (A's
# RESPONSE came through B's outbound socket, not B's listener).
grep -q "\"role\":\"node_A\".*\"event\":\"exit\",\"received\":1" "$LOG_A" \
    || { echo "FAIL node_A expected received=1 in exit" >&2; exit 9; }
grep -q "\"role\":\"node_B\".*\"event\":\"exit\",\"received\":0" "$LOG_B" \
    || { echo "FAIL node_B expected received=0 in exit" >&2; exit 10; }

echo "check-mesh-2node: PASS (real TCP QUERY → RESPONSE round-trip across ports $PORT_A ↔ $PORT_B)"
