#!/usr/bin/env python3
"""Build multi-source prompt manifest (JSON) for σ-gate v5."""
from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Any, Dict, List


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def load_truthfulqa(repo: Path, limit: int) -> List[Dict[str, Any]]:
    path = repo / "benchmarks/truthfulqa200/prompts.csv"
    pairs: List[Dict[str, Any]] = []
    with path.open(encoding="utf-8", newline="") as f:
        for i, row in enumerate(csv.DictReader(f)):
            if i >= limit:
                break
            pairs.append(
                {
                    "question": (row.get("question") or "").strip(),
                    "best_answer": (row.get("best_answer") or row.get("answer") or "").strip(),
                    "incorrect_answers": (row.get("best_incorrect_answer") or row.get("incorrect_answers") or "").strip(),
                    "source": "truthfulqa",
                }
            )
    return pairs


def load_triviaqa(limit: int) -> List[Dict[str, Any]]:
    from datasets import load_dataset

    ds = load_dataset(
        "trivia_qa",
        "unfiltered.nocontext",
        split="validation",
        streaming=True,
    )
    pairs: List[Dict[str, Any]] = []
    for row in ds:
        if len(pairs) >= limit:
            break
        q = (row.get("question") or "").strip()
        aliases = row.get("answer", {}).get("aliases") or []
        if not q or not aliases:
            continue
        pairs.append(
            {
                "question": q,
                "best_answer": str(aliases[0]),
                "incorrect_answers": "",
                "source": "triviaqa",
            }
        )
    return pairs


def load_natural_questions(limit: int) -> List[Dict[str, Any]]:
    try:
        from datasets import load_dataset

        ds = load_dataset("nq_open", split="validation", streaming=True)
    except Exception:
        return []
    pairs: List[Dict[str, Any]] = []
    for row in ds:
        if len(pairs) >= limit:
            break
        q = (row.get("question") or "").strip()
        ans = row.get("answer")
        if isinstance(ans, list) and ans:
            best = str(ans[0])
        elif isinstance(ans, str):
            best = ans
        else:
            best = ""
        if not q or not best:
            continue
        pairs.append(
            {
                "question": q,
                "best_answer": best,
                "incorrect_answers": "",
                "source": "natural_questions",
            }
        )
    return pairs


def load_halueval(limit: int) -> List[Dict[str, Any]]:
    from datasets import load_dataset

    ds = load_dataset("pminervini/HaluEval", "qa", split="data", streaming=True)
    pairs: List[Dict[str, Any]] = []
    for row in ds:
        if len(pairs) >= limit:
            break
        q = (row.get("question") or "").strip()
        right = (row.get("right_answer") or "").strip()
        hal = (row.get("hallucinated_answer") or "").strip()
        if not q or not right or not hal:
            continue
        pairs.append(
            {
                "question": q,
                "best_answer": right,
                "incorrect_answers": hal,
                "source": "halueval",
            }
        )
    return pairs


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--per-dataset", type=int, default=80, help="Max rows per HF source (TruthfulQA capped separately).")
    args = ap.parse_args()
    repo = _repo()
    per = int(args.per_dataset)

    all_pairs: List[Dict[str, Any]] = []
    loaders = [
        ("truthfulqa", lambda: load_truthfulqa(repo, min(per, 200))),
        ("triviaqa", lambda: load_triviaqa(per)),
        ("natural_questions", lambda: load_natural_questions(per)),
        ("halueval", lambda: load_halueval(per)),
    ]
    for name, fn in loaders:
        print(f"Loading {name}...")
        try:
            chunk = fn()
            all_pairs.extend(chunk)
            print(f"  -> {len(chunk)} rows")
        except Exception as e:
            print(f"  -> FAILED: {e}", file=sys.stderr)

    out = repo / "benchmarks/sigma_gate_v5/intermediate/multi_dataset.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(all_pairs, indent=2), encoding="utf-8")
    print(f"\nTotal rows: {len(all_pairs)} -> {out}")


if __name__ == "__main__":
    main()
