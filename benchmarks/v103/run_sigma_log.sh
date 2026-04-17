#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v103 — ONE eval pass that persists the full σ-profile per candidate.
#
# Runs the three multiple-choice tasks (arc_easy / arc_challenge /
# truthfulqa_mc2) at n=500 each, identical to v102's 2026-04-17 pass
# except it also writes a σ sidecar JSONL per task so that
# `compute_rc_metrics.py` can post-hoc sweep τ ∈ [0, 1] with no
# additional forward passes.
#
# The τ-sweep itself is deterministic on the sidecars; all ten curves
# in docs/v103/RESULTS.md come from this single ~2 h run.
#
# Flags:
#   --limit N     lm-eval --limit (default 500, smoke: 50)
#   --tasks CSV   comma-separated task names (default:
#                 arc_easy,arc_challenge,truthfulqa_mc2)
#   --out DIR     output root (default benchmarks/v103/results)
set -eo pipefail
cd "$(dirname "$0")/../.."

LIMIT=500
TASKS="arc_easy,arc_challenge,truthfulqa_mc2"
OUT="benchmarks/v103/results"

while [ $# -gt 0 ]; do
    case "$1" in
        --limit) LIMIT="$2"; shift 2;;
        --tasks) TASKS="$2"; shift 2;;
        --out)   OUT="$2"; shift 2;;
        -h|--help)
            sed -n '1,25p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
        *) echo "run_sigma_log.sh: unknown flag: $1" >&2; exit 2;;
    esac
done

GGUF="$PWD/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf"
BRIDGE="$PWD/creation_os_v101"

if [ ! -f "$GGUF" ];    then echo "missing $GGUF"    >&2; exit 3; fi
if [ ! -x "$BRIDGE" ];  then echo "missing $BRIDGE — make standalone-v101-real" >&2; exit 3; fi
if [ ! -x .venv-bitnet/bin/python ]; then
    echo "missing .venv-bitnet — see run_eval.sh" >&2; exit 3
fi

source .venv-bitnet/bin/activate

# Include the editable lm-eval source directly (Python 3.12 may skip .pth
# files under macOS-provenance'd folders); see benchmarks/v103/check_v103.sh.
LMEVAL_SRC="$PWD/third_party/lm-eval"
mkdir -p "$OUT"
if [ -f "$LMEVAL_SRC/lm_eval/__init__.py" ]; then
    export PYTHONPATH="$LMEVAL_SRC:$PWD/benchmarks/v103:$PWD/benchmarks/v102:${PYTHONPATH:-}"
else
    export PYTHONPATH="$PWD/benchmarks/v103:$PWD/benchmarks/v102:${PYTHONPATH:-}"
fi

if ! python -c 'import lm_eval' >/dev/null 2>&1; then
    pip install -q -e third_party/lm-eval >/dev/null
fi

echo "================================================================"
echo "== v103 σ-log run — $(date -Iseconds)"
echo "== tasks=$TASKS  limit=$LIMIT"
echo "================================================================"

IFS=',' read -r -a TASK_ARR <<<"$TASKS"
for TASK in "${TASK_ARR[@]}"; do
    SIDECAR="$OUT/samples_${TASK}_sigma.jsonl"
    rm -f "$SIDECAR"
    export COS_V103_SIGMA_SIDECAR="$SIDECAR"
    echo ""
    echo "---- [$TASK, limit=$LIMIT]  $(date -Iseconds) ----"
    python benchmarks/v103/run_lm_eval_v103.py \
        --model creation_os_v103 \
        --model_args "bridge=$BRIDGE,gguf=$GGUF,n_ctx=2048,task_tag=$TASK" \
        --tasks "$TASK" \
        --limit "$LIMIT" \
        --batch_size 1 \
        --output_path "$OUT/lm_eval/$TASK" \
        --log_samples \
        2>&1 | tail -15
done

echo ""
echo "== all σ-log runs done: $(date -Iseconds)"
echo "== sidecars: $OUT/samples_<task>_sigma.jsonl"
