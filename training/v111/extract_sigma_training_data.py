# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2026 Spektre Labs / Lauri Rainio
"""
v111.3 — extract σ-supervised training data from v104 sidecars.

Produces a MLX-LM `lora`-compatible JSONL with fields `prompt` /
`completion` from the existing v103 / v104 n=5000 sidecars.  Labelling
rule (directly from the directive):

    high σ  (sigma_product > HIGH_TAU)   → "I don't know."
    low  σ  (sigma_product < LOW_TAU )   → recorded model answer
    in-between                           → dropped (ambiguous)

This is σ-self-distillation: the model itself provides both the features
(σ profile) and the labels (its own correct / uncertain responses),
supplemented by an explicit "I don't know." token for the high-σ cases.
No external graders, no human labels, fully reproducible from the
sidecars already in this tree.

Usage
-----
    .venv-bitnet/bin/python training/v111/extract_sigma_training_data.py \
        --sidecars benchmarks/v104/n5000_results \
        --out      training/v111/data/sigma_abstain.jsonl

Flags:
    --high-tau  0.12   geometric-mean σ above which the label becomes IDK
                       (tuned for v105 sigma_product distribution on BitNet
                       b1.58 2B-4T; empirical P90 on arc_challenge+TruthfulQA
                       is ≈ 0.10, max ≈ 0.27).
    --low-tau   0.05   geometric-mean σ below which we keep the answer
                       (≈ P25 on the same distributions).
    --require-correct  if set, low-σ examples must also be acc==1.  ON by
                       default because only self-distilling CORRECT answers
                       is safe; a confident wrong answer should not become
                       supervision.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
from typing import Dict, Iterable, List, Optional, Tuple

IDK = "I don't know."
EPS = 1e-6

# v111 sigma_product helper (must match src/v101/sigma_channels.c).
def sigma_product(profile: List[float]) -> float:
    lp = [math.log(max(float(x), EPS)) for x in profile]
    return math.exp(sum(lp) / len(lp))


# ---------------------------- sidecar loader ---------------------------- #

def _iter_sidecar(path: str) -> Iterable[dict]:
    with open(path, "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


def _load_sidecar_index(sidecar: str) -> Dict[Tuple[int, int], dict]:
    idx: Dict[Tuple[int, int], dict] = {}
    for row in _iter_sidecar(sidecar):
        idx[(int(row["doc_index"]), int(row["cand_index"]))] = row
    return idx


# ---------------------------- lm-eval loader ---------------------------- #

def _iter_lm_eval_samples(task_dir: str) -> Iterable[dict]:
    """Yield lm-eval sample rows from the freshest file under `task_dir`."""
    candidates = []
    for root, _dirs, files in os.walk(task_dir):
        for f in files:
            if f.startswith(f"samples_") and f.endswith(".jsonl"):
                candidates.append(os.path.join(root, f))
    if not candidates:
        return
    candidates.sort(key=os.path.getmtime, reverse=True)
    with open(candidates[0], "r", encoding="utf-8") as fh:
        for line in fh:
            line = line.strip()
            if not line:
                continue
            try:
                yield json.loads(line)
            except json.JSONDecodeError:
                continue


def _extract_prompt_completion(sample: dict, cand_index: int
                               ) -> Optional[Tuple[str, str]]:
    """Dig the (ctx, cont) for the chosen argmax-ll candidate out of the
    lm-eval `arguments` dict.  Returns None if the row is missing fields."""
    args = sample.get("arguments")
    if isinstance(args, list):
        if cand_index >= len(args):
            return None
        pair = args[cand_index]
        if not isinstance(pair, (list, tuple)) or len(pair) < 2:
            return None
        return str(pair[0]), str(pair[1])
    if isinstance(args, dict):
        key = f"gen_args_{cand_index}"
        node = args.get(key)
        if not isinstance(node, dict):
            return None
        return str(node.get("arg_0", "")), str(node.get("arg_1", ""))
    return None


def _argmax_ll_candidate(sample: dict) -> int:
    resps = sample.get("filtered_resps") or sample.get("resps") or []
    lls: List[float] = []
    for pair in resps:
        if isinstance(pair, (list, tuple)) and len(pair) >= 1:
            v = pair[0]
            if isinstance(v, (int, float)):
                lls.append(float(v))
            elif isinstance(v, (list, tuple)) and v and isinstance(v[0], (int, float)):
                lls.append(float(v[0]))
    if not lls:
        return 0
    return max(range(len(lls)), key=lambda i: lls[i])


# ------------------------------ driver --------------------------------- #

def build(sidecars_root: str,
          out_path: str,
          high_tau: float,
          low_tau: float,
          require_correct: bool,
          limit_per_task: int) -> dict:
    stats = {"tasks": {}, "total_idk": 0, "total_answer": 0}
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as out:
        for fname in os.listdir(sidecars_root):
            if not fname.startswith("samples_") or not fname.endswith("_sigma.jsonl"):
                continue
            task = fname[len("samples_"):-len("_sigma.jsonl")]
            sidecar_path = os.path.join(sidecars_root, fname)
            task_dir = os.path.join(sidecars_root, "lm_eval", task)
            if not os.path.isdir(task_dir):
                continue

            sidecar = _load_sidecar_index(sidecar_path)
            kept_idk = 0
            kept_ans = 0
            seen = 0
            for sample in _iter_lm_eval_samples(task_dir):
                seen += 1
                doc_id = int(sample["doc_id"])
                cand   = _argmax_ll_candidate(sample)
                key = (doc_id, cand)
                row = sidecar.get(key) or sidecar.get((doc_id, 0))
                if row is None or "sigma_profile" not in row:
                    continue
                sp = sigma_product(row["sigma_profile"])
                acc = float(sample.get("acc", 0.0))

                pc = _extract_prompt_completion(sample, cand)
                if pc is None:
                    continue
                ctx, cont = pc
                if not ctx:
                    continue

                record: Optional[dict] = None
                if sp > high_tau:
                    record = {"prompt": ctx.rstrip() + "\n",
                              "completion": " " + IDK,
                              "sigma_product": round(sp, 6),
                              "label": "idk", "task": task}
                    kept_idk += 1
                elif sp < low_tau and ((not require_correct) or acc >= 0.5):
                    record = {"prompt": ctx.rstrip() + "\n",
                              "completion": cont if cont.startswith(" ") else " " + cont,
                              "sigma_product": round(sp, 6),
                              "label": "answer", "task": task}
                    kept_ans += 1

                if record is not None:
                    out.write(json.dumps(record, ensure_ascii=False) + "\n")
                    if kept_idk + kept_ans >= limit_per_task:
                        break
            stats["tasks"][task] = {"seen": seen, "idk": kept_idk, "answer": kept_ans}
            stats["total_idk"]    += kept_idk
            stats["total_answer"] += kept_ans
    return stats


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--sidecars", default="benchmarks/v104/n5000_results")
    ap.add_argument("--out",       default="training/v111/data/sigma_abstain.jsonl")
    ap.add_argument("--high-tau",  type=float, default=0.12)
    ap.add_argument("--low-tau",   type=float, default=0.05)
    ap.add_argument("--require-correct", action="store_true", default=True)
    ap.add_argument("--no-require-correct", dest="require_correct",
                    action="store_false")
    ap.add_argument("--limit-per-task", type=int, default=5000)
    args = ap.parse_args()

    if not os.path.isdir(args.sidecars):
        print(f"extract: sidecars not found at {args.sidecars}",
              file=sys.stderr)
        return 2
    stats = build(args.sidecars, args.out,
                  args.high_tau, args.low_tau,
                  args.require_correct, args.limit_per_task)
    print(json.dumps(stats, indent=2))
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
