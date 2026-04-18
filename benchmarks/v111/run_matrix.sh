#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v111 — run the frontier parity matrix end-to-end.
#
# What this does:
#   1. re-analyses ALL existing sidecars (v104/n5000_results for arc_easy /
#      arc_challenge / truthfulqa_mc2 at n=5000, any v111/results rows
#      for newer tasks) with the six v111 gating signals + paired bootstrap
#      ΔAURCC vs entropy, Bonferroni α = 0.05 / 24 = 0.00208.
#
#   2. if a task argument is passed and its sidecar is missing, it will
#      attempt to GENERATE a fresh σ-sidecar via the v103 backend.  This
#      requires BitNet weights + .venv-bitnet + creation_os_v101 real-mode
#      build.  If any of those are absent the task is left PENDING and the
#      matrix is still emitted.
#
# Usage:
#   bash benchmarks/v111/run_matrix.sh                 # analyse only
#   bash benchmarks/v111/run_matrix.sh hellaswag       # + hellaswag n=1000
#   bash benchmarks/v111/run_matrix.sh all             # + all 4 families at full limits
#
# Full-limit runs for MMLU / GSM8K / HumanEval are documented in
# docs/v111/THE_FRONTIER_MATRIX.md §4 with exact CLI; they are NOT part
# of the merge-gate as each takes many hours on an M3.
set -eo pipefail
cd "$(dirname "$0")/../.."

TARGET="${1:-analyse-only}"
N_BOOT="${V111_N_BOOT:-2000}"
PY=".venv-bitnet/bin/python"

# ---------------------------------------------------------------- run -- #

generate_sidecar () {
    local task="$1"
    local limit="$2"
    local out="benchmarks/v111/results"

    local gguf="$PWD/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf"
    local bridge="$PWD/creation_os_v101"

    if [ ! -f "$gguf" ];    then echo "v111: SKIP $task — missing $gguf"; return 0; fi
    if [ ! -x "$bridge" ];  then echo "v111: SKIP $task — run 'make standalone-v101-real' first"; return 0; fi
    if [ ! -x "$PY" ];      then echo "v111: SKIP $task — no .venv-bitnet"; return 0; fi

    mkdir -p "$out"
    local sidecar="$out/samples_${task}_sigma.jsonl"
    export COS_V103_SIGMA_SIDECAR="$sidecar"

    local lmeval_src="$PWD/third_party/lm-eval"
    export PYTHONPATH="$lmeval_src:$PWD/benchmarks/v103:$PWD/benchmarks/v102:${PYTHONPATH:-}"

    echo "---- v111 σ-log: $task (limit=$limit)  $(date -Iseconds) ----"
    rm -f "$sidecar"
    "$PY" benchmarks/v103/run_lm_eval_v103.py \
        --model creation_os_v103 \
        --model_args "bridge=$bridge,gguf=$gguf,n_ctx=2048,task_tag=$task" \
        --tasks "$task" \
        --limit "$limit" \
        --batch_size 1 \
        --output_path "$out/lm_eval/$task" \
        --log_samples \
        --cache_requests true \
        --seed 0
}

case "$TARGET" in
    analyse-only)
        ;;
    hellaswag)
        generate_sidecar hellaswag 1000
        ;;
    mmlu)
        # 57 subtasks, 5-shot, very heavy — single-subject smoke
        generate_sidecar mmlu_high_school_psychology 500
        ;;
    gsm8k)
        echo "v111: gsm8k requires a generate_until backend with σ logging."
        echo "     See docs/v111/THE_FRONTIER_MATRIX.md §4.2 for the repro command."
        ;;
    humaneval)
        echo "v111: humaneval requires a code-execution harness."
        echo "     See docs/v111/THE_FRONTIER_MATRIX.md §4.3 for the repro command."
        ;;
    all)
        generate_sidecar hellaswag 1000
        ;;
    *)
        echo "run_matrix.sh: unknown target '$TARGET'"
        echo "try: analyse-only | hellaswag | mmlu | gsm8k | humaneval | all"
        exit 2
        ;;
esac

# ------------------------------------------------------------ matrix -- #

if [ ! -x "$PY" ]; then
    echo "v111: SKIP matrix build — no .venv-bitnet python"
    exit 0
fi

"$PY" benchmarks/v111/frontier_matrix.py \
    --tasks arc_challenge,arc_easy,truthfulqa_mc2,hellaswag \
    --n-boot "$N_BOOT"

echo ""
echo "v111 frontier matrix:  benchmarks/v111/results/frontier_matrix.md"
echo "v111 rc curves:        benchmarks/v111/results/rc_curves.json"
