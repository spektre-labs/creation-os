#!/usr/bin/env bash
# Run Eleuther lm-eval-harness against an OpenAI-compatible completions endpoint.
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
#
# Standard TruthfulQA MC2 / ARC-Challenge / HellaSwag numbers use model type
# `local-completions`, which parses legacy `choices[].logprobs.token_logprobs`
# from POST /v1/completions. Stock Ollama (tested with /v1/completions + numeric
# logprobs) does not return that structure, so loglikelihood tasks fail there.
# Point LM_EVAL_BASE_URL at llama.cpp llama-server, vLLM, or any server that
# returns OpenAI-style completion logprobs, or run generative benchmarks only.
#
# Usage:
#   TASK=truthfulqa_mc2 LIMIT=200 ./scripts/real/run_lm_eval_gemma_ollama.sh
#   TASK=arc_challenge NUM_FEWSHOT=25 ./scripts/real/run_lm_eval_gemma_ollama.sh
#   TASK=hellaswag NUM_FEWSHOT=10 ./scripts/real/run_lm_eval_gemma_ollama.sh
#
# Env:
#   LM_EVAL_BASE_URL   default http://127.0.0.1:11434/v1/completions
#   LM_EVAL_MODEL      default gemma3:4b
#   LM_EVAL_TOKENIZER  HF tokenizer id (default: TinyLlama/TinyLlama-1.1B-Chat-v1.0)
#                      Use a Gemma tokenizer id if you have HF access; length
#                      estimates must match your server context.
#   TASK               truthfulqa_mc2 | arc_challenge | hellaswag
#   OUT_ROOT           default benchmarks/lm_eval
#   LIMIT              optional --limit (smoke only; omit for full eval)
#   NUM_FEWSHOT        optional override (ARC default 25, HellaSwag 10)
#
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

if ! command -v lm-eval >/dev/null 2>&1; then
  echo "error: lm-eval not on PATH (pip install 'lm-eval[api]' transformers)" >&2
  exit 1
fi

TASK="${TASK:-truthfulqa_mc2}"
OUT_ROOT="${OUT_ROOT:-benchmarks/lm_eval}"
BASE_URL="${LM_EVAL_BASE_URL:-http://127.0.0.1:11434/v1/completions}"
MODEL="${LM_EVAL_MODEL:-gemma3:4b}"
TOKENIZER="${LM_EVAL_TOKENIZER:-TinyLlama/TinyLlama-1.1B-Chat-v1.0}"

case "$TASK" in
  truthfulqa_mc2) OUT_SUB="truthfulqa" ;;
  arc_challenge)  OUT_SUB="arc" ;;
  hellaswag)      OUT_SUB="hellaswag" ;;
  *)
    echo "error: unknown TASK=$TASK (use truthfulqa_mc2, arc_challenge, hellaswag)" >&2
    exit 2
    ;;
esac

OUT_PATH="${OUT_ROOT}/${OUT_SUB}"
mkdir -p "$OUT_PATH"

MODEL_ARGS="model=${MODEL},base_url=${BASE_URL},num_concurrent=1,tokenized_requests=False,tokenizer=${TOKENIZER}"

EXTRA=()
if [[ -n "${LIMIT:-}" ]]; then
  EXTRA+=(--limit "$LIMIT")
fi

if [[ "$TASK" == "arc_challenge" ]]; then
  EXTRA+=(--num_fewshot "${NUM_FEWSHOT:-25}")
elif [[ "$TASK" == "hellaswag" ]]; then
  EXTRA+=(--num_fewshot "${NUM_FEWSHOT:-10}")
fi

echo "=== lm-eval run ==="
echo "TASK=$TASK OUT_PATH=$OUT_PATH"
echo "BASE_URL=$BASE_URL"
echo "MODEL=$MODEL TOKENIZER=$TOKENIZER"
echo ""

lm-eval run \
  --model local-completions \
  --model_args "$MODEL_ARGS" \
  --tasks "$TASK" \
  "${EXTRA[@]}" \
  --output_path "$OUT_PATH" \
  --log_samples

echo ""
echo "Done. Samples: $OUT_PATH/local-completions/samples_${TASK}_*.jsonl"
echo "Aggregated JSON under $OUT_PATH/local-completions/"
