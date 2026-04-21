#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Export TruthfulQA (generation split) to pipeline_bench JSONL format.

Each output line:
  {"id", "category", "prompt", "truth", "local_correct", "api_correct"}

Oracle labels ``local_correct`` / ``api_correct`` are **not** computed by a
judge model here: defaults are pessimistic for ``local_correct`` (False)
and optimistic for ``api_correct`` (True) so hybrid accuracy reflects
σ-routing against a fixed always-API ceiling until you replace labels with
scores from a real evaluation harness.

Requires: ``pip install datasets``

Example::

    python3 scripts/real/export_truthfulqa_jsonl.py \\
        --split validation \\
        --out benchmarks/pipeline/data/truthfulqa_817.jsonl
"""

from __future__ import annotations

import argparse
import json
import pathlib
import sys


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--split", default="validation", help="HF split name")
    ap.add_argument(
        "--out",
        required=True,
        type=pathlib.Path,
        help="Output JSONL path (parent dirs created).",
    )
    args = ap.parse_args()
    try:
        from datasets import load_dataset  # type: ignore
    except ImportError:
        print(
            "export_truthfulqa_jsonl.py: install Hugging Face `datasets` "
            "(pip install datasets)",
            file=sys.stderr,
        )
        return 2

    ds = load_dataset("truthful_qa", "generation", split=args.split)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    n = 0
    with open(args.out, "w", encoding="utf-8") as fh:
        for i, row in enumerate(ds):
            q = str(row["question"]).strip()
            truth = str(row["best_answer"]).strip()
            cat = str(row.get("category", "")).strip()
            correct = [str(x).strip() for x in row.get("correct_answers", []) if str(x).strip()]
            incorrect = [str(x).strip() for x in row.get("incorrect_answers", []) if str(x).strip()]
            rec = {
                "id": f"tqa-{i + 1:04d}",
                "category": cat,
                "prompt": q,
                "truth": truth,
                "correct_answers": correct,
                "incorrect_answers": incorrect,
                "local_correct": False,
                "api_correct": True,
            }
            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
            n += 1
    print(f"wrote {n} rows -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
