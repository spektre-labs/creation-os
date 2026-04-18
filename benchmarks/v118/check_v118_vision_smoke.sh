#!/usr/bin/env bash
# v118 σ-Vision smoke test.
#
# Verifies the v118.0 interface surface:
#   1. self-test passes (parse + project + σ-gate + JSON)
#   2. data: URL with base64 payload is parsed and decoded
#   3. uniform-byte "image" triggers σ > τ abstain
#   4. structured (peaky-histogram) bytes pass the σ-gate
#
# Pure-C, no SigLIP weights, no network — merge-gate safe.

set -euo pipefail

BIN=${BIN:-./creation_os_v118_vision}
if [ ! -x "$BIN" ]; then
    echo "[v118] $BIN missing — run 'make creation_os_v118_vision' first" >&2
    exit 1
fi

echo "[v118] self-test"
"$BIN" --self-test

TMP=$(mktemp -d -t cos_v118.XXXXXX)
trap 'rm -rf "$TMP"' EXIT

# 1) data: URL parse
cat > "$TMP/body.json" <<'EOF'
{"type":"image_url","image_url":{"url":"data:image/png;base64,SEVMTE8K"}}
EOF
OUT=$("$BIN" --parse-body "$TMP/body.json")
echo "[v118] parse: $OUT"
echo "$OUT" | grep -q '"source":1'  || { echo "[v118] FAIL: source should be data url"; exit 1; }
echo "$OUT" | grep -q '"decoded":6' || { echo "[v118] FAIL: expected 6 decoded bytes"; exit 1; }

# 2) Uniform byte image → high σ → abstain
printf '%b' "$(awk 'BEGIN{for(i=0;i<4096;i++) printf("\\x%02x", i%256)}')" \
    > "$TMP/uniform.bin"
U=$("$BIN" --project "$TMP/uniform.bin" 0.55)
echo "[v118] uniform: $U"
echo "$U" | grep -q '"abstained":true' \
    || { echo "[v118] FAIL: uniform image must abstain" >&2; exit 1; }
echo "$U" | grep -q '"projection_channel":"embedding_entropy"' \
    || { echo "[v118] FAIL: expected embedding_entropy channel" >&2; exit 1; }

# 3) Peaky (one byte dominates) image → low σ → pass
python3 -c 'open("'"$TMP"'/peaky.bin","wb").write(bytes(((i%100==0) and (i%255+1) or 0) for i in range(4096)))'
P=$("$BIN" --project "$TMP/peaky.bin" 0.55)
echo "[v118] peaky: $P"
echo "$P" | grep -q '"abstained":false' \
    || { echo "[v118] FAIL: peaky image must not abstain" >&2; exit 1; }

# 4) JSON shape covers the required fields
for needle in '"sigma_product":' '"tau_vision":' '"embedding_dim":2048' '"content_hash":' '"preview":\['; do
    if ! echo "$P" | grep -qE "$needle"; then
        echo "[v118] FAIL: json missing $needle" >&2
        exit 1
    fi
done

echo "[v118] OK"
