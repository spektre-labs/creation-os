#!/usr/bin/env bash
#
# PROD-5 smoke test: σ-Signal graceful shutdown + σ-Persist handoff.
#
# Exercises the three code paths:
#   1. --self-test       → unit-level hook + flag + uninstall
#   2. --simulate SIG*   → flag-latching without actual kill(2)
#   3. --persist-shutdown → hook writes final state into SQLite
#   4. --daemon | kill -TERM → real signal delivery wakes the
#                              handler, drains through the hook,
#                              child exits 0 (not ≥128).
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_signal"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# ---- 1. self-test ---------------------------------------------------------
"$BIN" --self-test | grep -q '"self_test_rc":0' \
    || { echo "FAIL: --self-test rc != 0" >&2; exit 3; }

# ---- 2. simulated SIGINT / SIGTERM / SIGHUP -------------------------------
for S in SIGINT SIGTERM SIGHUP; do
    O="$("$BIN" --simulate $S)"
    grep -q "\"signal\":\"$S\"" <<<"$O" \
        || { echo "FAIL: simulate $S didn't surface signal" >&2; echo "$O"; exit 4; }
    grep -q '"hook_fired":true' <<<"$O" \
        || { echo "FAIL: simulate $S didn't fire hook" >&2; echo "$O"; exit 5; }
    grep -q '"pending_after":0' <<<"$O" \
        || { echo "FAIL: simulate $S left pending flag" >&2; echo "$O"; exit 6; }
done

# ---- 3. persist-shutdown saves final state to SQLite ---------------------
DB="/tmp/cos_signal_check.db"
rm -f "$DB" "${DB}-wal" "${DB}-shm"
O="$("$BIN" --persist-shutdown "$DB")"
grep -q '"hook_fired":true' <<<"$O" \
    || { echo "FAIL: persist-shutdown didn't fire hook" >&2; echo "$O"; exit 7; }
grep -q '"saved":true' <<<"$O" \
    || { echo "FAIL: persist-shutdown didn't save" >&2; echo "$O"; exit 8; }

if command -v sqlite3 >/dev/null 2>&1; then
    val="$(sqlite3 "$DB" "SELECT value FROM pipeline_meta WHERE key='last_shutdown_signum';")"
    [[ "$val" == "15" ]] \
        || { echo "FAIL: expected '15' in DB, got '$val'" >&2; exit 9; }
    grc="$(sqlite3 "$DB" "SELECT value FROM pipeline_meta WHERE key='graceful';")"
    [[ "$grc" == "1" ]] \
        || { echo "FAIL: expected graceful=1 in DB, got '$grc'" >&2; exit 10; }
    echo "  sqlite3: last_shutdown_signum=$val, graceful=$grc"
fi

# ---- 4. real kill(2) → daemon drains ---------------------------------------
TMPLOG="$(mktemp)"
"$BIN" --daemon >"$TMPLOG" 2>&1 &
PID=$!
# Wait until the child declares it is up.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    grep -q '"mode":"daemon"' "$TMPLOG" 2>/dev/null && break
    sleep 0.1
done
kill -TERM "$PID"
wait "$PID"
RC=$?
[[ $RC -eq 0 ]] \
    || { echo "FAIL: daemon exit=$RC (expected 0)" >&2; cat "$TMPLOG"; exit 11; }
grep -q '"drained":true' "$TMPLOG" \
    || { echo "FAIL: daemon did not drain" >&2; cat "$TMPLOG"; exit 12; }
grep -q '"signum":15' "$TMPLOG" \
    || { echo "FAIL: daemon didn't see SIGTERM=15" >&2; cat "$TMPLOG"; exit 13; }
rm -f "$TMPLOG"

echo "check-sigma-signal: PASS (self-test + simulate + persist + real-SIGTERM drain)"
