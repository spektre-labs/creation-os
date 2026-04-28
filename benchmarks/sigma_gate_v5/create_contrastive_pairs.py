#!/usr/bin/env python3
"""Turn labeled generations into TruthfulQA-style CSV for adapt_lsd.py."""
from __future__ import annotations

import csv
import json
import sys
from pathlib import Path
from typing import Dict, List


def _repo() -> Path:
    return Path(__file__).resolve().parents[2]


def main() -> None:
    repo = _repo()
    jpath = repo / "benchmarks/sigma_gate_v5/intermediate/labeled_rows.jsonl"
    if not jpath.is_file():
        print(f"Missing {jpath}", file=sys.stderr)
        sys.exit(2)

    rows: List[Dict[str, str]] = []
    with jpath.open(encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line:
                continue
            r = json.loads(line)
            qm = (r.get("question_for_model") or "").strip()
            ref = (r.get("reference") or "").strip()
            resp = (r.get("response") or "").strip()
            neg_meta = (r.get("incorrect_meta") or "").strip()
            src = (r.get("source") or "").strip()
            if not qm or not ref:
                continue
            wrong_text = resp if not r.get("semantic_correct") else neg_meta
            if not wrong_text:
                continue
            rows.append(
                {
                    "id": r.get("id", ""),
                    "category": src,
                    "question": qm,
                    "best_answer": ref,
                    "best_incorrect_answer": wrong_text[:4000],
                    "incorrect_answers": wrong_text[:4000],
                    "source": src,
                }
            )

    out = repo / "benchmarks/sigma_gate_v5/intermediate/multi_contrastive.csv"
    out.parent.mkdir(parents=True, exist_ok=True)
    fields = ["id", "category", "question", "best_answer", "best_incorrect_answer", "incorrect_answers", "source"]
    with out.open("w", encoding="utf-8", newline="") as fp:
        w = csv.DictWriter(fp, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for row in rows:
            w.writerow({k: row.get(k, "") for k in fields})

    print(f"Wrote {len(rows)} contrastive rows -> {out}")


if __name__ == "__main__":
    main()
