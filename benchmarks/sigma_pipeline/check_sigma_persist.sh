#!/usr/bin/env bash
#
# PROD-3 smoke test: σ-Persist SQLite WAL roundtrip.
#
# Pinned facts:
#   - self_test_rc == 0
#   - schema_version == 1 (COS_PERSIST_SCHEMA_VERSION)
#   - WAL mode actually activated (inspect the .db with PRAGMA)
#   - row counts after demo: meta≥2, engrams=2, tau=3, cost=2
#   - codename "Genesis" reads back intact after close/reopen
#   - a second run over the same DB does NOT duplicate engrams
#     (UPSERT behaviour)
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_persist"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

DB="/tmp/cos_persist_demo.db"
rm -f "$DB" "${DB}-wal" "${DB}-shm"

OUT1="$($BIN)"
OUT2="$($BIN)"

for fact in \
    '"self_test_rc":0' \
    '"pass":true' \
    '"enabled":true' \
    '"schema_version":1' \
    '"codename":"Genesis"' \
    '"engrams":2' \
    '"tau_calibration":3' \
    '"cost_ledger":2'
do
    grep -q -F "$fact" <<<"$OUT1" \
        || { echo "FAIL: run#1 missing '$fact'" >&2; echo "$OUT1"; exit 3; }
done

# Engram count must still be 2 after a 2nd run (UPSERT, not append).
grep -q -F '"engrams":2' <<<"$OUT2" \
    || { echo "FAIL: engram UPSERT duplicated on 2nd run" >&2; echo "$OUT2"; exit 4; }

# cost_ledger is an append-only tape — must grow to 4 after run#2.
python3 - "$OUT2" <<'PY'
import json, sys
j = json.loads(sys.argv[1])
assert j["self_test_rc"] == 0, j
assert j["schema_version"] == 1, j
assert j["rows"]["cost_ledger"] == 4, j  # 2 + 2, append-only
assert j["rows"]["engrams"] == 2, j      # UPSERT, no dup
assert j["rows"]["tau_calibration"] == 3, j
print("check-sigma-persist: JSON shape OK (upsert engrams, append cost, roundtrip)")
PY

# Confirm WAL mode is set on the file.
if command -v sqlite3 >/dev/null 2>&1; then
    jm="$(sqlite3 "$DB" 'PRAGMA journal_mode;')"
    [[ "$jm" == "wal" ]] \
        || { echo "FAIL: journal_mode = '$jm' (expected 'wal')" >&2; exit 5; }
    uv="$(sqlite3 "$DB" 'PRAGMA user_version;')"
    [[ "$uv" == "1" ]] \
        || { echo "FAIL: user_version = '$uv' (expected 1)" >&2; exit 6; }
    echo "  sqlite3: journal_mode=$jm, user_version=$uv"
fi

echo "check-sigma-persist: PASS (WAL + schema v1 + upsert/append + roundtrip)"
