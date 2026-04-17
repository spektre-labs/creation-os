#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "$0")/../.."
source .venv-bitnet/bin/activate
export PYTHONPATH="$PWD/benchmarks/v102:${PYTHONPATH:-}"
GGUF="$PWD/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf"
BRIDGE="$PWD/creation_os_v101"
OUT="benchmarks/v102/results"
mkdir -p "$OUT"

echo "================================================================"
echo "== v102 σ-Eval-Harness — σ-gated full run"
echo "== started: $(date -Iseconds)"
echo "================================================================"

for TASK in arc_easy arc_challenge truthfulqa_mc2; do
    case "$TASK" in
        arc_easy|arc_challenge) LIMIT=500;;
        truthfulqa_mc2)         LIMIT=500;;
    esac
    echo ""
    echo "---- [$TASK, limit=$LIMIT] $(date -Iseconds) ----"
    python benchmarks/v102/run_lm_eval.py \
        --model creation_os \
        --model_args "bridge=$BRIDGE,gguf=$GGUF,n_ctx=2048" \
        --tasks "$TASK" \
        --limit "$LIMIT" \
        --batch_size 1 \
        --output_path "$OUT/sigma_gated" \
        --log_samples \
        2>&1 | tail -15
done

echo ""
echo "== all tasks done: $(date -Iseconds)"
