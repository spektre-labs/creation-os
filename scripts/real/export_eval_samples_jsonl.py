#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Write lm-eval-style sample JSONL lines (doc + ids) for cos replay / local QA.

Each output line matches the shape consumed by ``scripts/real/benchmark_gemma3.py``
(minimal fields: doc_id, doc, target, arguments, resps, filtered_resps, filter,
metrics, doc_hash, prompt_hash, target_hash).

Examples::

  python3 scripts/real/export_eval_samples_jsonl.py --task truthfulqa_mc2 --limit 200 \\
      --out benchmarks/lm_eval/receipts/truthfulqa_mcq.json
  python3 scripts/real/export_eval_samples_jsonl.py --task arc_challenge --limit 200 \\
      --out benchmarks/lm_eval/receipts/arc_challenge.json
  python3 scripts/real/export_eval_samples_jsonl.py --task hellaswag --limit 200 \\
      --out benchmarks/lm_eval/receipts/hellaswag.json
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

try:
    from datasets import load_dataset
except ImportError as e:
    print("error: pip install datasets", file=sys.stderr)
    raise SystemExit(2) from e


def _row(doc_id: int, doc: Dict[str, Any], target: str) -> Dict[str, Any]:
    doc_s = json.dumps(doc, sort_keys=True, ensure_ascii=False)
    h = hashlib.sha256(doc_s.encode("utf-8")).hexdigest()[:16]
    return {
        "doc_id": doc_id,
        "doc": doc,
        "target": target,
        "arguments": {},
        "resps": [],
        "filtered_resps": [],
        "filter": "none",
        "metrics": [],
        "doc_hash": h,
        "prompt_hash": h,
        "target_hash": hashlib.sha256(target.encode("utf-8")).hexdigest()[:16],
    }


def load_truthfulqa_mc2(limit: int) -> List[Dict[str, Any]]:
    ds = load_dataset("truthfulqa/truthful_qa", "multiple_choice", split="validation")
    out: List[Dict[str, Any]] = []
    for i in range(min(limit, len(ds))):
        row = ds[i]
        doc = {"question": row["question"], "mc2_targets": dict(row["mc2_targets"])}
        out.append(_row(i, doc, "0"))
    return out


def load_arc_challenge(limit: int) -> List[Dict[str, Any]]:
    ds = load_dataset("allenai/ai2_arc", "ARC-Challenge", split="validation")
    out: List[Dict[str, Any]] = []
    for i in range(min(limit, len(ds))):
        row = ds[i]
        doc = {
            "question": row["question"],
            "choices": dict(row["choices"]),
            "answerKey": row["answerKey"],
        }
        out.append(_row(i, doc, row["answerKey"]))
    return out


def load_hellaswag(limit: int) -> List[Dict[str, Any]]:
    ds = load_dataset("Rowan/hellaswag", split="validation")
    out: List[Dict[str, Any]] = []

    def proc(ex: Dict[str, Any]) -> Dict[str, Any]:
        ctx = ex["ctx_a"] + " " + str(ex["ctx_b"]).capitalize()
        q = (str(ex["activity_label"]).strip() + ": " + ctx).replace(" [title]", ". ")
        choices = [str(x).strip() for x in ex["endings"]]
        gold = int(ex["label"])
        return {"query": q, "choices": choices, "gold": gold}

    for i in range(min(limit, len(ds))):
        doc = proc(ds[i])
        out.append(_row(i, doc, str(doc["gold"])))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--task", choices=("truthfulqa_mc2", "arc_challenge", "hellaswag"), required=True)
    ap.add_argument("--limit", type=int, default=200)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    if args.task == "truthfulqa_mc2":
        rows = load_truthfulqa_mc2(args.limit)
    elif args.task == "arc_challenge":
        rows = load_arc_challenge(args.limit)
    else:
        rows = load_hellaswag(args.limit)

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps(rows, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )
    print("wrote", len(rows), "records ->", args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
