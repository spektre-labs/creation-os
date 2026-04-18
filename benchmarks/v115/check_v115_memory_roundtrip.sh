#!/usr/bin/env bash
# v115 σ-Memory: roundtrip smoke test
#
# Exercises the standalone creation_os_v115_memory binary against a
# temporary SQLite database:
#   1. self-test (embedder + in-memory schema + σ-weighted recall)
#   2. write two episodic rows with different σ values
#   3. search returns JSON with expected shape (top_k, lambda, results)
#   4. σ-weighted ranking: low-σ row ranks above high-σ row
#
# When the build was compiled with -DCOS_V115_NO_SQLITE=1 the binary
# exits 0 with a "memory unavailable" diagnostic; this script treats
# that as a clean SKIP rather than a failure so merge-gate never stalls
# on missing libsqlite3-dev.

set -euo pipefail

BIN=${BIN:-./creation_os_v115_memory}
if [ ! -x "$BIN" ]; then
    echo "[v115] $BIN missing — build with 'make creation_os_v115_memory'" >&2
    exit 1
fi

TMP=$(mktemp -d -t cos_v115.XXXXXX)
trap 'rm -rf "$TMP"' EXIT
DB="$TMP/mem.sqlite"

echo "[v115] self-test"
if ! OUT=$("$BIN" --self-test 2>&1); then
    echo "$OUT" >&2
    echo "[v115] self-test FAIL" >&2
    exit 1
fi
printf '%s\n' "$OUT"

if echo "$OUT" | grep -q "memory unavailable on this build"; then
    echo "[v115] SKIP (no libsqlite3 at build time)"
    exit 0
fi

echo "[v115] write episodic rows"
"$BIN" --db "$DB" --write-episodic "Paris is the capital of France" \
       --sigma 0.05 --session test --tags "geo,confident" >/dev/null
"$BIN" --db "$DB" --write-episodic "Paris is the capital of France" \
       --sigma 0.80 --session test --tags "geo,uncertain" >/dev/null
"$BIN" --db "$DB" --write-episodic "Python lists are ordered mutable sequences" \
       --sigma 0.10 --session test --tags "code,confident" >/dev/null

COUNTS=$("$BIN" --db "$DB" --counts)
echo "[v115] counts: $COUNTS"
echo "$COUNTS" | grep -q '"episodic":3' \
    || { echo "[v115] FAIL: expected episodic=3 got $COUNTS" >&2; exit 1; }

echo "[v115] σ-weighted search"
RES=$("$BIN" --db "$DB" --search "What is the capital of France?" --top-k 3)
printf '%s\n' "$RES"

for needle in '"top_k":3' '"lambda":' '"results":' 'capital of France'; do
    if ! echo "$RES" | grep -q "$needle"; then
        echo "[v115] FAIL: expected '$needle' in $RES" >&2
        exit 1
    fi
done

# Extract the first two sigma_product fields — the higher-σ duplicate
# must rank *below* the lower-σ one.  (The JSON is single-line, so a
# simple awk/sed over the sorted order works.)
SIGS=$(printf '%s' "$RES" \
     | tr '{}' '\n' \
     | grep '"sigma_product"' \
     | sed -E 's/.*"sigma_product":([0-9.]+).*/\1/' \
     | head -n 2)

FIRST=$(echo  "$SIGS" | sed -n '1p')
SECOND=$(echo "$SIGS" | sed -n '2p')
echo "[v115] first two σ: $FIRST then $SECOND"
awk -v a="$FIRST" -v b="$SECOND" \
    'BEGIN{ exit !(a+0 < b+0) }' \
    || { echo "[v115] FAIL: σ-weighting did not prefer low σ (first=$FIRST second=$SECOND)" >&2; exit 1; }

echo "[v115] OK"
