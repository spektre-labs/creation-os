#!/usr/bin/env bash
# v113 σ-Sandbox — merge-gate smoke check.
set -u
cd "$(dirname "$0")/../.."

BIN=./creation_os_v113_sandbox
if [ ! -x "$BIN" ]; then
    echo "[check-v113] building creation_os_v113_sandbox"
    if ! make -s creation_os_v113_sandbox 2>/dev/null; then
        echo "[check-v113] SKIP: toolchain missing"
        exit 0
    fi
fi

echo "[check-v113] 1/3 pure-C self-test"
"$BIN" --self-test || { echo "[check-v113] FAIL self-test"; exit 1; }

echo "[check-v113] 2/3 σ-gated reject (high σ, code never runs)"
REQ_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t cos)
cat > "$REQ_DIR/gated.json" <<'JSON'
{"code":"print('should not run')","language":"python","timeout":3,"sigma_product":0.85,"tau_code":0.60}
JSON
OUT="$("$BIN" --execute "$REQ_DIR/gated.json" 2>&1 || true)"
echo "$OUT" | grep -q '"gated_out":true' \
    || { echo "[check-v113] FAIL: gated response missing"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q '"executed":false' \
    || { echo "[check-v113] FAIL: code ran despite gate"; echo "$OUT"; exit 1; }

echo "[check-v113] 3/3 live echo (bash; σ disabled)"
cat > "$REQ_DIR/live.json" <<'JSON'
{"code":"echo v113_alive","language":"bash","timeout":5}
JSON
OUT="$("$BIN" --execute "$REQ_DIR/live.json")"
echo "$OUT" | grep -q '"executed":true' \
    || { echo "[check-v113] FAIL: live executed flag"; echo "$OUT"; exit 1; }
echo "$OUT" | grep -q 'v113_alive' \
    || { echo "[check-v113] FAIL: stdout capture"; echo "$OUT"; exit 1; }

rm -rf "$REQ_DIR"
echo "[check-v113] OK"
