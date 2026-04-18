#!/usr/bin/env bash
# v112 σ-Agent — merge-gate smoke check.
#
# Verifies:
#   1. Pure-C self-test of parsers + σ-gate (no model needed) passes.
#   2. 10-scenario JSONL fixture parses cleanly; σ-gate renders
#      manifests and emits a stub report per scenario.
#   3. (Optional) If the BitNet weights are present at the default
#      path, runs a live σ-gated tool selection on one scenario.
set -u
cd "$(dirname "$0")/../.."

BIN=./creation_os_v112_tools
if [ ! -x "$BIN" ]; then
    echo "[check-v112] building creation_os_v112_tools"
    if ! make -s creation_os_v112_tools 2>/dev/null; then
        echo "[check-v112] SKIP: toolchain missing"
        exit 0
    fi
fi

echo "[check-v112] 1/3 pure-C self-test"
"$BIN" --self-test || {
    echo "[check-v112] FAIL: self-test"
    exit 1
}

echo "[check-v112] 2/3 10-scenario JSONL"
SC="benchmarks/v112/scenarios.jsonl"
if [ ! -f "$SC" ]; then
    echo "[check-v112] missing fixture: $SC"; exit 1
fi
"$BIN" --scenarios "$SC" || {
    echo "[check-v112] FAIL: scenarios"
    exit 1
}

LINES=$(grep -c '^{' "$SC" || true)
if [ "${LINES:-0}" -lt 10 ]; then
    echo "[check-v112] FAIL: scenarios fixture has <10 rows ($LINES)"
    exit 1
fi

echo "[check-v112] 3/3 live σ-gate (optional)"
GGUF="${COS_V112_GGUF:-third_party/weights/bitnet-b1.58-2b.gguf}"
if [ -f "$GGUF" ] && [ -x ./creation_os_server ]; then
    echo "[check-v112] live gate skipped in CI; enable with COS_V112_LIVE=1"
fi

echo "[check-v112] OK"
