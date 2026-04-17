#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v104 — follow-up σ-log pass at `--limit 5000` on the three MC tasks.
#
# lm-eval caps `--limit` to the actual dataset size, so for these tasks
# n=5000 = use everything:
#   truthfulqa_mc2  → 817 docs  (validation)     ≈  5 700 ll-calls
#   arc_challenge   → 1 172 docs (test)          ≈  4 700 ll-calls
#   arc_easy        → 2 376 docs (test)          ≈  9 500 ll-calls
# Total ≈ 19 900 ll-calls ≈ 5 – 6 h wall on Apple M3 / Metal / bitnet.cpp i2_s.
# (n=500 ran 7 591 calls in ~1 h 30 min; scaling linearly.)
#
# Output goes to benchmarks/v104/n5000_results/ so it never overwrites the
# v103 n=500 sidecar.  compute_operator_rcc.py, channel_analysis.py and
# hybrid_sweep.py all take `--results` so they can be pointed at either
# directory; the v104 RESULTS.md reports both.
set -eo pipefail
cd "$(dirname "$0")/../.."

LIMIT=5000
TASKS="truthfulqa_mc2,arc_challenge,arc_easy"
OUT="benchmarks/v104/n5000_results"

while [ $# -gt 0 ]; do
    case "$1" in
        --limit) LIMIT="$2"; shift 2;;
        --tasks) TASKS="$2"; shift 2;;
        --out)   OUT="$2"; shift 2;;
        -h|--help)
            sed -n '1,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0;;
        *) echo "run_n5000_sigma_log.sh: unknown flag: $1" >&2; exit 2;;
    esac
done

GGUF="$PWD/models/BitNet-b1.58-2B-4T/ggml-model-i2_s.gguf"
BRIDGE="$PWD/creation_os_v101"

[ -f "$GGUF" ]    || { echo "missing $GGUF"    >&2; exit 3; }
[ -x "$BRIDGE" ]  || { echo "missing $BRIDGE — make standalone-v101-real" >&2; exit 3; }
[ -x .venv-bitnet/bin/python ] || { echo "missing .venv-bitnet" >&2; exit 3; }

source .venv-bitnet/bin/activate
mkdir -p "$OUT" "$OUT/lm_eval"

LMEVAL_SRC="$PWD/third_party/lm-eval"
if [ -f "$LMEVAL_SRC/lm_eval/__init__.py" ]; then
    export PYTHONPATH="$LMEVAL_SRC:$PWD/benchmarks/v103:${PYTHONPATH:-}"
else
    export PYTHONPATH="$PWD/benchmarks/v103:${PYTHONPATH:-}"
fi

echo "================================================================"
echo "== v104 σ-log n=$LIMIT run — $(date -Iseconds)"
echo "== tasks=$TASKS  (capped to dataset size by lm-eval)"
echo "== out:  $OUT"
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
        2>&1 | tail -20
done

echo ""
echo "== v104 σ-log done: $(date -Iseconds)"
echo "== run post-hoc analysis:"
echo "   python benchmarks/v104/compute_operator_rcc.py --results $OUT"
echo "   python benchmarks/v104/channel_analysis.py     --results $OUT"
echo "   python benchmarks/v104/hybrid_sweep.py         --results $OUT"
