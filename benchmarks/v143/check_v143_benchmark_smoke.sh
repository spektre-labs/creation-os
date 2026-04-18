#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v143 merge-gate: 5-category benchmark smoke.
#
#   1. Self-test passes (5 categories green on synthetic data).
#   2. --run produces a JSON object with all 5 category blocks and
#      the v143.0-synthetic tier label.
#   3. --out writes benchmarks/v143/creation_os_benchmark.json and
#      the file round-trips through grep/jq-free shape checks.
#
set -Eeuo pipefail

BIN="${BIN:-./creation_os_v143_benchmark}"
[[ -x "$BIN" ]] || { echo "[v143] $BIN not built — run 'make $BIN'"; exit 2; }

OUT_JSON="benchmarks/v143/creation_os_benchmark.json"

echo "[v143] 1/4 self-test"
"$BIN" --self-test >/tmp/cos_v143_self.log 2>&1 || {
    cat /tmp/cos_v143_self.log; exit 1; }

echo "[v143] 2/4 --run emits 5-category JSON"
OUT="$("$BIN" --run --seed 0xC057BB)"
printf '%s\n' "${OUT:0:600}..."
for key in '"calibration":{' '"abstention":{' '"swarm":{' '"learning":{' '"adversarial":{' '"tier":"v143.0-synthetic"'; do
    echo "$OUT" | grep -q "$key" \
        || { echo "[v143] FAIL missing $key in --run output"; exit 1; }
done

echo "[v143] 3/4 --out writes JSON to ${OUT_JSON}"
rm -f "$OUT_JSON"
"$BIN" --run --seed 0xC057BB --out "$OUT_JSON" >/dev/null
[[ -s "$OUT_JSON" ]] || { echo "[v143] FAIL JSON file empty or missing"; exit 1; }
FILE="$(cat "$OUT_JSON")"
echo "[v143] file size: $(wc -c < "$OUT_JSON") bytes"
for key in '"ece":' '"coverage_at_95":' '"routing_accuracy":' \
           '"accuracy_before":' '"detection_at_fpr05":'; do
    echo "$FILE" | grep -q "$key" \
        || { echo "[v143] FAIL missing $key in written JSON"; exit 1; }
done

echo "[v143] 4/4 determinism check (same seed → byte-identical JSON)"
"$BIN" --run --seed 0xC057BB --out "/tmp/cos_v143_rerun.json" >/dev/null
diff -q "$OUT_JSON" "/tmp/cos_v143_rerun.json" \
    || { echo "[v143] FAIL benchmark is non-deterministic under fixed seed"; exit 1; }

echo "[v143] PASS — 5 categories + JSON shape + deterministic seed"
