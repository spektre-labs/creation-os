#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""BENCH-1: materialise Hugging Face rows for the SCI-6 suite (prompts only).

Writes one JSONL per dataset under ``benchmarks/suite/data/``:

  { "id": "...", "dataset": "arc_challenge", "prompt": "...",
    "gold": "C" }   # gold is dataset-specific (letter / numeric string / index)

Requires ``datasets`` + network on first materialisation (HF cache).

This script does **not** run inference — see ``run_suite_cos_chat_detail.py``.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from typing import Any, Dict, Iterator

try:
    from datasets import load_dataset
except ImportError as e:  # pragma: no cover
    print("export_suite_hf_rows: install datasets: pip install datasets", file=sys.stderr)
    raise SystemExit(2) from e


def _arc_prompt(row: Dict[str, Any]) -> str:
    q = row["question"]
    labels = row["choices"]["label"]
    texts = row["choices"]["text"]
    lines = [f"  {lab}) {txt}" for lab, txt in zip(labels, texts)]
    return (
        f"{q}\n\nChoose the correct answer. Reply with **only** the option letter "
        f"(one of {', '.join(labels)}).\n" + "\n".join(lines)
    )


def iter_arc(split_name: str, config: str, suite_name: str) -> Iterator[Dict[str, Any]]:
    ds = load_dataset("allenai/ai2_arc", config, split=split_name)
    for row in ds:
        yield {
            "id": str(row["id"]),
            "dataset": suite_name,
            "prompt": _arc_prompt(row),
            "gold": str(row["answerKey"]).strip().upper()[:1],
        }


def iter_gsm8k() -> Iterator[Dict[str, Any]]:
    ds = load_dataset("openai/gsm8k", "main", split="test")
    for i, row in enumerate(ds):
        q = row["question"].strip()
        ans = row["answer"]
        # final #### line is canonical numeric result
        gold = ""
        for line in str(ans).strip().splitlines():
            line = line.strip()
            if line.startswith("####"):
                gold = line.replace("####", "").strip()
                break
        yield {
            "id": f"gsm8k-{i}",
            "dataset": "gsm8k",
            "prompt": (
                f"{q}\n\nAnswer with the final numeric result only "
                f"(digits and optional minus sign), on the last line prefixed "
                f"exactly as #### <number>."
            ),
            "gold": gold,
        }


def iter_hellaswag(limit: int) -> Iterator[Dict[str, Any]]:
    ds = load_dataset("Rowan/hellaswag", split="validation")
    for i, row in enumerate(ds):
        if i >= limit:
            break
        ctx = row["ctx"]
        endings = row["endings"]
        lab = int(row["label"])
        letters = "ABCD"
        lines = [f"  {letters[j]}) {endings[j]}" for j in range(len(endings))]
        yield {
            "id": f"hs-{row.get('ind', i)}",
            "dataset": "hellaswag",
            "prompt": (
                f"{ctx}\n\nWhich ending makes the most sense? "
                f"Reply with **only** one letter A–D.\n" + "\n".join(lines)
            ),
            "gold": letters[lab] if 0 <= lab < 4 else str(lab),
        }


def write_jsonl(path: str, rows: Iterator[Dict[str, Any]]) -> int:
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    n = 0
    with open(path, "w", encoding="utf-8") as fp:
        for r in rows:
            fp.write(json.dumps(r, ensure_ascii=False) + "\n")
            n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--out-dir",
        default="benchmarks/suite/data",
        help="directory for *_rows.jsonl",
    )
    ap.add_argument(
        "--hellaswag-limit",
        type=int,
        default=500,
        help="validation subset size (default 500)",
    )
    args = ap.parse_args()
    root = args.out_dir

    n1 = write_jsonl(
        os.path.join(root, "arc_challenge_rows.jsonl"),
        iter_arc("test", "ARC-Challenge", "arc_challenge"),
    )
    n2 = write_jsonl(
        os.path.join(root, "arc_easy_rows.jsonl"),
        iter_arc("test", "ARC-Easy", "arc_easy"),
    )
    n3 = write_jsonl(os.path.join(root, "gsm8k_rows.jsonl"), iter_gsm8k())
    n4 = write_jsonl(
        os.path.join(root, "hellaswag_rows.jsonl"),
        iter_hellaswag(args.hellaswag_limit),
    )
    print(
        json.dumps(
            {
                "arc_challenge_rows": n1,
                "arc_easy_rows": n2,
                "gsm8k_rows": n3,
                "hellaswag_rows": n4,
                "out_dir": root,
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
