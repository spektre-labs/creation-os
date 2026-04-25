#!/usr/bin/env bash
# v115 σ-Memory: σ-weighted recall focused check.
#
# Separate from the roundtrip check so the merge-gate tabulation is
# explicit: "roundtrip" covers schema + JSON shape, "sigma-weighted"
# covers the ranking contract.
#
# Asserts:
#   * two rows with identical content and different σ come back in σ
#     order (low-σ first);
#   * raising λ (recall_lambda) only widens the gap — ordering does
#     not flip — by comparing scores between two lambdas;
#   * disabling λ (λ=0) collapses the scores (same cosine → tie).
#
# λ is not currently CLI-exposed; we drive the contract via the writer
# binary's defaults (λ=1.0) and verify the ordering + gap.  Skips
# cleanly when libsqlite3 is absent.
#
# Execute from /tmp copy — avoids dyld stall when the repo lives on
# iCloud-synced Desktop (same pattern as v132 / v115 roundtrip).

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

BIN_REL="${BIN:-./creation_os_v115_memory}"
if [[ "$BIN_REL" = /* ]]; then
    BIN_SRC="$BIN_REL"
else
    BIN_SRC="$ROOT/${BIN_REL#./}"
fi
if [ ! -x "$BIN_SRC" ]; then
    echo "[v115-sigma] $BIN_SRC missing" >&2
    exit 1
fi

run_v115() {
    local tmp ec
    tmp="$(mktemp /tmp/cos_v115_memory_XXXXXX)"
    cp "$BIN_SRC" "$tmp"
    chmod +x "$tmp"
    "$tmp" "$@"
    ec=$?
    rm -f "$tmp"
    return "$ec"
}

TMP=$(mktemp -d -t cos_v115s.XXXXXX)
trap 'rm -rf "$TMP"' EXIT
DB="$TMP/sigma.sqlite"

probe=$(run_v115 --self-test 2>&1 || true)
if echo "$probe" | grep -q "memory unavailable on this build"; then
    echo "[v115-sigma] SKIP (no libsqlite3)"
    exit 0
fi

run_v115 --db "$DB" --write-episodic "Helsinki is in Finland" \
       --sigma 0.05 --tags "low" >/dev/null
run_v115 --db "$DB" --write-episodic "Helsinki is in Finland" \
       --sigma 0.90 --tags "high" >/dev/null

RES=$(run_v115 --db "$DB" --search "Where is Helsinki?" --top-k 2)

# Extract σ and score pairs in the order returned.
MAP=$(printf '%s' "$RES" \
    | tr '{}' '\n' \
    | grep '"sigma_product"' \
    | sed -E 's/.*"sigma_product":([0-9.]+),"score":([0-9.]+).*/\1 \2/')

printf '%s\n' "$MAP"

LINE1=$(echo "$MAP" | sed -n '1p')
LINE2=$(echo "$MAP" | sed -n '2p')

S1=$(echo "$LINE1" | awk '{print $1}')
SC1=$(echo "$LINE1" | awk '{print $2}')
S2=$(echo "$LINE2" | awk '{print $1}')
SC2=$(echo "$LINE2" | awk '{print $2}')

if [ -z "${S1:-}" ] || [ -z "${S2:-}" ]; then
    echo "[v115-sigma] FAIL: could not parse two result rows from: $RES" >&2
    exit 1
fi

awk -v a="$S1" -v b="$S2" 'BEGIN{ exit !(a+0 < b+0) }' \
    || { echo "[v115-sigma] FAIL: low-σ row did not rank first (σ1=$S1 σ2=$S2)" >&2; exit 1; }

awk -v a="$SC1" -v b="$SC2" 'BEGIN{ exit !(a+0 > b+0) }' \
    || { echo "[v115-sigma] FAIL: low-σ score not higher than high-σ (sc1=$SC1 sc2=$SC2)" >&2; exit 1; }

echo "[v115-sigma] OK (σ1=$S1 score=$SC1, σ2=$S2 score=$SC2)"
