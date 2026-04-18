#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# v111.3 σ-SFT smoke — merge-gate safe.
#
# Validates:
#   (1) the σ-training-data extractor runs end-to-end on the v104 n=5000
#       sidecars and produces a non-empty JSONL with both `idk` and
#       `answer` labels present;
#   (2) the LoRA trainer script parses its arguments and emits a
#       well-formed mlx_lm.lora invocation.
#
# Does NOT:
#   - require MLX to be installed (smoke uses --dry-run);
#   - download or load any base model;
#   - touch the network or GPU.
#
# Full-run instructions are in docs/v111/THE_SIGMA_ABSTAIN_LORA.md.
set -eo pipefail
cd "$(dirname "$0")/../.."

PY="${COS_V111_PYTHON:-.venv-bitnet/bin/python}"
if [ ! -x "$PY" ]; then
    echo "check-v111-sft-smoke: SKIP (no $PY)"
    exit 0
fi

PYVER=$("$PY" -c 'import sys; print(sys.version_info >= (3,9))' 2>/dev/null || echo False)
if [ "$PYVER" != "True" ]; then
    echo "check-v111-sft-smoke: SKIP (Python < 3.9)"
    exit 0
fi

SIDECARS="benchmarks/v104/n5000_results"
if [ ! -d "$SIDECARS" ] || [ -z "$(ls -A "$SIDECARS"/samples_*_sigma.jsonl 2>/dev/null)" ]; then
    echo "check-v111-sft-smoke: SKIP (no v104 n=5000 sidecars at $SIDECARS)"
    exit 0
fi

DATA="training/v111/data/sigma_abstain.jsonl"
"$PY" training/v111/extract_sigma_training_data.py \
    --sidecars "$SIDECARS" --out "$DATA" --limit-per-task 500 >/dev/null

if [ ! -s "$DATA" ]; then
    echo "check-v111-sft-smoke: FAIL — extractor wrote empty $DATA"
    exit 1
fi

# Both labels should be present (confirmed by the extractor stats).
IDK_COUNT=$(grep -c '"label": "idk"' "$DATA" || true)
ANS_COUNT=$(grep -c '"label": "answer"' "$DATA" || true)
echo "v111.3 data:  total=$(wc -l < "$DATA")  idk=$IDK_COUNT  answer=$ANS_COUNT"
if [ "$IDK_COUNT" = "0" ] || [ "$ANS_COUNT" = "0" ]; then
    echo "check-v111-sft-smoke: FAIL — one label class is empty (need both)"
    exit 1
fi

# Dry-run the trainer — verifies argument assembly without installing MLX.
"$PY" training/v111/train_sigma_abstain.py --dry-run --iters 1 >/tmp/v111_trainer_cmd.log
if ! grep -q "mlx_lm.lora" /tmp/v111_trainer_cmd.log; then
    echo "check-v111-sft-smoke: FAIL — trainer did not emit mlx_lm.lora invocation"
    sed -n '1,10p' /tmp/v111_trainer_cmd.log
    exit 1
fi
if ! grep -q -- "--adapter-path" /tmp/v111_trainer_cmd.log; then
    echo "check-v111-sft-smoke: FAIL — trainer missing --adapter-path"
    exit 1
fi

# If MLX is installed AND the user wants a live smoke (COS_V111_SFT_LIVE=1),
# run a single-iter LoRA against a tiny local cache.  OFF by default so
# merge-gate stays fast and offline.
if [ "${COS_V111_SFT_LIVE:-0}" = "1" ] && "$PY" -c 'import mlx.core' >/dev/null 2>&1; then
    echo "v111.3: running 1-iter LoRA smoke (COS_V111_SFT_LIVE=1)"
    "$PY" training/v111/train_sigma_abstain.py --iters 1 --steps-per-eval 1 --save-every 1
fi

echo "check-v111-sft-smoke: PASS"
