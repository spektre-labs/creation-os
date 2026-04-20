#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
benchmarks/v111/synthesize_hellaswag_lmeval.py

Rebuild the `samples_hellaswag_<run>.jsonl` file that frontier_matrix.py
needs to populate the hellaswag row of the v111 parity matrix.

Why this script exists (v0 contract):
  The hellaswag σ-sidecar already stores, per (doc_index, cand_index),
  the BitNet log-likelihood that was produced during the v103 σ-logging
  run.  What was *not* archived alongside it is the lm-eval samples
  file, and without that file frontier_matrix.py leaves hellaswag as
  PENDING.  We reconstruct acc per doc purely from:
    (a) the σ-sidecar ll values (real BitNet outputs, not synthetic), and
    (b) the public hellaswag validation gold labels loaded from the
        local pyarrow cache.

No model is re-run.  No log-likelihood is fabricated.  The output row
format exactly matches what lm-eval v0.4.x emits so that existing v103
loaders succeed unmodified.
"""

from __future__ import annotations

import argparse
import collections
import json
import os
import sys
from datetime import datetime, timezone


# --------------------------------------------------------------- IO ----- #

def _load_sidecar(path: str) -> dict:
    per_doc: dict = collections.defaultdict(dict)
    with open(path) as f:
        for ln in f:
            r = json.loads(ln)
            if r.get("task") != "hellaswag":
                continue
            per_doc[int(r["doc_index"])][int(r["cand_index"])] = r
    return dict(per_doc)


def _load_gold_labels(arrow_path: str) -> list:
    import pyarrow.ipc as ipc
    with open(arrow_path, "rb") as f:
        table = ipc.open_stream(f).read_all()
    labels = table.column("label").to_pylist()
    return [int(x) for x in labels]


def _emit_row(doc_id: int, cand_rows: dict, gold: int) -> dict:
    """Produce a row shaped like lm-eval's samples_hellaswag_<run>.jsonl."""
    cand_ids = sorted(cand_rows.keys())
    resps = []
    for ci in cand_ids:
        row = cand_rows[ci]
        resps.append([f"{row['ll']:.7f}", str(bool(row.get("is_greedy", False)))])
    lls = [float(resps[i][0]) for i in range(len(resps))]
    pred = max(range(len(lls)), key=lambda i: lls[i])
    acc = 1.0 if pred == gold else 0.0
    return {
        "doc_id": doc_id,
        "target": gold,
        "resps": resps,
        "filtered_resps": resps,
        "acc": acc,
        "doc": {"label": str(gold)},
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sidecar",
                    default="benchmarks/v111/results/samples_hellaswag_sigma.jsonl")
    ap.add_argument("--validation-arrow",
                    default=os.path.expanduser(
                        "~/.cache/huggingface/datasets/Rowan___hellaswag/"
                        "default/0.0.0/218ec52e09a7e7462a5400043bb9a69a41d06b76/"
                        "hellaswag-validation.arrow"))
    ap.add_argument("--out-root", default="benchmarks/v111/results/lm_eval/hellaswag")
    ap.add_argument("--min-candidates", type=int, default=4,
                    help="drop docs whose sidecar has fewer candidates")
    args = ap.parse_args()

    if not os.path.isfile(args.sidecar):
        print(f"synth: missing σ sidecar {args.sidecar}", file=sys.stderr)
        return 2
    if not os.path.isfile(args.validation_arrow):
        print(f"synth: missing hellaswag arrow {args.validation_arrow}",
              file=sys.stderr)
        return 2

    per_doc = _load_sidecar(args.sidecar)
    gold = _load_gold_labels(args.validation_arrow)

    kept = 0
    dropped = 0
    rows = []
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
    run_slug = "synth_from_sidecar"
    out_dir = os.path.join(args.out_root, run_slug)
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, f"samples_hellaswag_{stamp}.jsonl")
    with open(out_path, "w") as f:
        for r in rows:
            f.write(json.dumps(r) + "\n")

    acc_mean = sum(r["acc"] for r in rows) / max(1, len(rows))
    print(f"synth: wrote {out_path}")
    print(f"       docs_kept={kept}  docs_dropped={dropped}  acc={acc_mean:.4f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
