#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# v102 σ-Eval-Harness driver.
#
# Runs EleutherAI lm-evaluation-harness against BitNet-b1.58-2B-4T in two
# configurations and writes per-task JSON outputs to benchmarks/v102/results:
#
#   1. baseline     — vanilla llama.cpp / HF-transformers backend
#   2. sigma_gated  — routes through creation_os_v101 so every loglikelihood
#                     call is scored by the real BitNet model *and* tagged
#                     with the eight Creation OS σ-channels
#
# RESULTS.md is written / appended by the caller once both runs are done.
#
# Flags:
#   --tasks CSV        tasks to run (default: arc_easy,truthfulqa_mc2,gsm8k)
#   --limit N          pass --limit to lm_eval (default: unset → full set)
#   --num-fewshot N    default 0 for MC2, auto for GSM8K
#   --skip-baseline    skip the baseline run (useful once it is cached)
#   --skip-sigma       skip the σ-gated run (useful while debugging baseline)
set -eo pipefail
cd "$(dirname "$0")/../.."

TASKS="arc_easy,truthfulqa_mc2,gsm8k"
declare -a LIMIT_ARG=()
declare -a NFEW_ARG=()
SKIP_BASELINE=0
SKIP_SIGMA=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tasks)         TASKS="$2"; shift 2;;
        --limit)         LIMIT_ARG=(--limit "$2"); shift 2;;
        --num-fewshot)   NFEW_ARG=(--num_fewshot "$2"); shift 2;;
        --skip-baseline) SKIP_BASELINE=1; shift;;
        --skip-sigma)    SKIP_SIGMA=1; shift;;
        -h|--help)
            sed -n '1,30p' "$0" | sed 's/^# \{0,1\}//'
            exit 0;;
        *)
            echo "run_eval.sh: unknown flag: $1" >&2; exit 2;;
    esac
done

MODEL_DIR="models/BitNet-b1.58-2B-4T"
TOKENIZER_DIR="models/BitNet-b1.58-2B-4T-tokenizer"
GGUF="$MODEL_DIR/ggml-model-i2_s.gguf"
OUT="benchmarks/v102/results"
mkdir -p "$OUT"

if [ ! -f "$GGUF" ]; then
    echo "run_eval.sh: missing $GGUF.  hf download microsoft/BitNet-b1.58-2B-4T-gguf --local-dir $MODEL_DIR" >&2
    exit 3
fi
if [ ! -d "$TOKENIZER_DIR" ]; then
    echo "run_eval.sh: missing $TOKENIZER_DIR.  hf download microsoft/bitnet-b1.58-2B-4T --include 'tokenizer*' --include '*.json' --local-dir $TOKENIZER_DIR" >&2
    exit 3
fi
if [ ! -x .venv-bitnet/bin/python ]; then
    echo "run_eval.sh: no .venv-bitnet/bin/python.  python3 -m venv .venv-bitnet && .venv-bitnet/bin/pip install -e third_party/lm-eval" >&2
    exit 3
fi
if [ ! -x ./creation_os_v101 ]; then
    echo "run_eval.sh: missing ./creation_os_v101.  make standalone-v101-real" >&2
    exit 3
fi

source .venv-bitnet/bin/activate

# Install lm-eval editable if not already done (fast on subsequent runs).
if ! python -c 'import lm_eval' >/dev/null 2>&1; then
    pip install -q -e third_party/lm-eval >/dev/null
fi

export PYTHONPATH="$PWD/benchmarks/v102:${PYTHONPATH:-}"

if [ $SKIP_BASELINE -eq 0 ]; then
    echo "== baseline: BitNet + vanilla lm-eval (gguf via llama.cpp) ========================"
    python -m lm_eval \
        --model gguf \
        --model_args "base_url=http://localhost:8080,tokenizer=$TOKENIZER_DIR" \
        --tasks "$TASKS" \
        --batch_size 1 \
        --output_path "$OUT/baseline" \
        --log_samples \
        "${LIMIT_ARG[@]}" "${NFEW_ARG[@]}" \
        2>&1 | tee "$OUT/baseline.log" \
        || echo "baseline: note — llama-server must be running on :8080 (bash benchmarks/v102/start_llama_server.sh)"
fi

if [ $SKIP_SIGMA -eq 0 ]; then
    echo "== σ-gated: BitNet via creation_os_v101 --ll / --gen ==============================="
    python -m lm_eval \
        --model creation_os \
        --model_args "bridge=$PWD/creation_os_v101,gguf=$PWD/$GGUF" \
        --tasks "$TASKS" \
        --batch_size 1 \
        --output_path "$OUT/sigma_gated" \
        --log_samples \
        "${LIMIT_ARG[@]}" "${NFEW_ARG[@]}" \
        2>&1 | tee "$OUT/sigma_gated.log"
fi

echo "run_eval.sh: results under $OUT/{baseline,sigma_gated}/"
