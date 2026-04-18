#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v106 σ-Server — real-model curl benchmark.
#
# Run `creation_os_server` against a loaded GGUF (BitNet b1.58 2B-4T
# by default), then issue a handful of /v1/chat/completions calls
# and sample /v1/sigma-profile after each.  Prints a table of
# wall-time, sigma_product, abstain-flag per prompt.
#
# This is NOT part of merge-gate.  It is the curl-test the v106
# acceptance criterion in the directive calls out:
#     "toimiiko curl-testi loopback:lla?"
# Success criterion: the non-streaming path returns a JSON
# response with `creation_os.sigma_product` in [0,1] and a non-
# zero `creation_os.sigma_profile` for every prompt.
set -euo pipefail

cd "$(dirname "$0")/../.."

BIN="${COS_V106_SERVER_BIN:-./creation_os_server}"
GGUF="${COS_V106_GGUF:-models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf}"
PORT="${COS_V106_SERVER_PORT:-18780}"

if [ ! -x "$BIN" ]; then
    echo "bench-v106: $BIN not built; run 'make standalone-v106-real' first"
    exit 1
fi
if [ ! -f "$GGUF" ]; then
    echo "bench-v106: GGUF $GGUF missing; run the installer or huggingface-cli download"
    exit 1
fi

LOG="$(mktemp -t cos_v106_bench_XXXXXX.log)"
"$BIN" --host 127.0.0.1 --port "$PORT" --gguf "$GGUF" --n-ctx 2048 \
       > "$LOG" 2>&1 &
PID=$!
cleanup() { kill "$PID" 2>/dev/null || true; wait "$PID" 2>/dev/null || true; rm -f "$LOG"; }
trap cleanup EXIT

# Wait for bind.
for _ in $(seq 1 30); do
    if curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then break; fi
    sleep 0.3
done

if ! curl -fsS "http://127.0.0.1:$PORT/health" >/dev/null 2>&1; then
    echo "bench-v106: server never bound; log:"
    cat "$LOG"
    exit 1
fi

echo "bench-v106: server up at http://127.0.0.1:$PORT"

PROMPTS=(
    "What is 1+1?"
    "In one sentence, explain quantum entanglement."
    "Name three planets in our solar system."
    "What colour is the sky?"
    "Translate 'hello' to Finnish."
)

echo
printf "%-60s  %8s  %10s  %9s  %s\n" \
       "prompt" "wall_s" "sigma_prod" "abstained" "first_16_of_reply"

for q in "${PROMPTS[@]}"; do
    t0=$(python3 -c 'import time;print(time.time())')
    RESP=$(curl -fsS \
        -H 'Content-Type: application/json' \
        -d "{\"model\":\"bitnet-b1.58-2B\",\"messages\":[{\"role\":\"user\",\"content\":$(printf %s "$q" | python3 -c 'import json,sys;print(json.dumps(sys.stdin.read()))')}],\"max_tokens\":64}" \
        "http://127.0.0.1:$PORT/v1/chat/completions")
    t1=$(python3 -c 'import time;print(time.time())')
    WALL=$(python3 -c "print(f'{($t1 - $t0):.2f}')")

    # Extract sigma_product, abstained, content first-16 chars via python.
    read SIGMA_PROD ABSTAIN REPLY < <(python3 -c "
import json, sys
try:
    j = json.loads('''$RESP''')
    cos = j.get('creation_os', {})
    msg = j.get('choices', [{}])[0].get('message', {}).get('content', '')
    print(f\"{cos.get('sigma_product', float('nan')):.4f} {cos.get('abstained', False)} {msg[:16]!r}\")
except Exception as e:
    print(f'nan False error:{e!r}')
")

    printf "%-60s  %8s  %10s  %9s  %s\n" \
           "${q:0:60}" "$WALL" "$SIGMA_PROD" "$ABSTAIN" "$REPLY"
done

echo
echo "bench-v106: last σ snapshot —"
curl -fsS "http://127.0.0.1:$PORT/v1/sigma-profile" | python3 -m json.tool
