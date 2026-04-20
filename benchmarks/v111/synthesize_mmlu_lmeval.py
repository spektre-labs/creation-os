#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
benchmarks/v111/synthesize_mmlu_lmeval.py

Mirror of `synthesize_hellaswag_lmeval.py` for MMLU subtasks.

Why this script exists:
  The v103 σ-logging run records the four per-candidate log-likelihoods
  in the sidecar but the lm-eval post-processing step (which writes
  `lm_eval/<task>/<run>/samples_<task>_*.jsonl`) occasionally stalls on
  systems with torch < 2.4 (lm-eval prints "Disabling PyTorch …" and
  waits indefinitely for a subprocess that never emits).  The σ data
  we actually need is already in the sidecar, and the gold answer is
  in the locally-cached HF `cais/mmlu/<subject>` arrow file, so we
  reconstruct the lm-eval samples row from those two sources without
  re-running any inference.

No log-likelihood is fabricated.  The argmax-ll candidate is compared
to the gold answer and marked correct / incorrect exactly as lm-eval
would.  The output row shape matches v103's `_load_lm_eval_samples`
expectations so `frontier_matrix._per_doc_rows` succeeds unchanged.
"""

from __future__ import annotations

import argparse
import collections
import glob
import json
import os
import sys
from datetime import datetime, timezone

MMLU_SPLIT = "test"           # lm-eval MMLU default
HF_ROOT = os.path.expanduser("~/.cache/huggingface/datasets/cais___mmlu")


def _load_sidecar(path: str, task: str) -> dict:
    per_doc: dict = collections.defaultdict(dict)
    with open(path) as f:
        for ln in f:
            r = json.loads(ln)
            if r.get("task") != task:
                continue
            per_doc[int(r["doc_index"])][int(r["cand_index"])] = r
    return dict(per_doc)


def _find_arrow(subject: str, split: str = MMLU_SPLIT) -> str | None:
    pat = os.path.join(HF_ROOT, subject, "*", "*", f"mmlu-{split}.arrow")
    hits = sorted(glob.glob(pat))
    return hits[-1] if hits else None


def _load_gold(subject: str) -> list[int]:
    import pyarrow.ipc as ipc
    p = _find_arrow(subject)
    if not p or not os.path.isfile(p):
        raise FileNotFoundError(
            f"mmlu synth: cannot find local arrow for cais/mmlu/{subject}. "
            f"Expected glob: {HF_ROOT}/{subject}/*/*/mmlu-{MMLU_SPLIT}.arrow. "
            "Re-run `bash benchmarks/v111/run_matrix.sh mmlu-micro` first "
            "so HF caches the dataset locally.")
    with open(p, "rb") as f:
        t = ipc.open_stream(f).read_all()
    return [int(x) for x in t.column("answer").to_pylist()]


def _emit_row(doc_id: int, cand_rows: dict, gold: int) -> dict:
    cand_ids = sorted(cand_rows.keys())
    resps = []
    for ci in cand_ids:
        row = cand_rows[ci]
        resps.append([f"{row['ll']:.7f}", str(bool(row.get("is_greedy", False)))])
    lls = [float(r[0]) for r in resps]
    pred = max(range(len(lls)), key=lambda i: lls[i])
    return {
        "doc_id": doc_id,
        "target": gold,
        "resps": resps,
        "filtered_resps": resps,
        "acc": 1.0 if pred == gold else 0.0,
        "doc": {"label": str(gold)},
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--task", default="mmlu_abstract_algebra",
                    help="lm-eval task slug, e.g. mmlu_abstract_algebra")
    ap.add_argument("--sidecar", default=None,
                    help="path to samples_<task>_sigma.jsonl")
    ap.add_argument("--out-root", default=None)
    ap.add_argument("--min-candidates", type=int, default=4)
    args = ap.parse_args()

    subject = args.task[len("mmlu_"):] if args.task.startswith("mmlu_") else args.task
    sidecar = args.sidecar or (
        f"benchmarks/v111/results/samples_{args.task}_sigma.jsonl")
    out_root = args.out_root or f"benchmarks/v111/results/lm_eval/{args.task}"

    if not os.path.isfile(sidecar):
        print(f"mmlu synth: missing sidecar {sidecar}", file=sys.stderr)
        return 2

    per_doc = _load_sidecar(sidecar, args.task)
    gold = _load_gold(subject)

    rows = []
    kept = 0
    dropped = 0
    for doc_id in sorted(per_doc):
        cand_rows = per_doc[doc_id]
        if len(cand_rows) < args.min_candidates:
            dropped += 1
            continue
        if doc_id >= len(gold):
            dropped += 1
            continue
        rows.append(_emit_row(doc_id, cand_rows, gold[doc_id]))
        kept += 1

    stamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H-%M-%S.%f")
    out_dir = os.path.join(out_root, "synth_from_sidecar")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f"samples_{args.task}_{stamp}.jsonl")
    with open(out_path, "w") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")

    acc = sum(r["acc"] for r in rows) / max(1, len(rows))
    print(f"mmlu synth: wrote {out_path}")
    print(f"            kept={kept} dropped={dropped} acc={acc:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
