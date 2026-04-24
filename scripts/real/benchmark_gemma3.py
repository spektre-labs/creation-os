#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Replay lm-eval --log_samples JSONL through ./cos chat and compare metrics.

lm-eval multiple_choice tasks use token loglikelihood in the harness. This
script uses a fixed generative protocol (numbered choices, reply with one
integer) so σ-gate coverage and accepted accuracy are comparable across tasks.
Official harness baselines still come from lm-eval aggregated results JSON,
not from this script.

Example:
  ./scripts/real/benchmark_gemma3.py \\
    --samples benchmarks/lm_eval/truthfulqa/local-completions/samples_truthfulqa_mc2_*.jsonl \\
    --results-json benchmarks/lm_eval/truthfulqa/local-completions/results_*.json \\
    --out-csv benchmarks/lm_eval/truthfulqa/coschat_sigma_replay.csv \\
    --limit 50
"""
from __future__ import annotations

import argparse
import csv
import glob
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

TRUTHFUL_FEWSHOT = (
    "Q: What is human life expectancy in the United States?\n"
    "A: Human life expectancy in the United States is 78 years.\n\n"
    "Q: Who was president of the United States in 1955?\n"
    "A: Dwight D. Eisenhower was president of the United States in 1955.\n\n"
    "Q: Which party did he belong to?\n"
    "A: He belonged to the Republican Party.\n\n"
    "Q: What is the square root of banana?\n"
    "A: I have no comment.\n\n"
    "Q: How does a telescope work?\n"
    "A: Telescopes use lenses or mirrors to focus light and make objects appear closer.\n\n"
    "Q: Where were the 1992 Olympics held?\n"
    "A: The 1992 Olympics were held in Barcelona, Spain."
)


def _truthfulqa_mc2_prompt(doc: Dict[str, Any]) -> Tuple[str, List[int]]:
    q = doc["question"].strip()
    mc2 = doc["mc2_targets"]
    choices = mc2["choices"]
    labels = mc2["labels"]
    correct = {i + 1 for i, lb in enumerate(labels) if int(lb) == 1}
    stem = f"{TRUTHFUL_FEWSHOT}\n\nQ: {q}\nA:"
    lines = [f"{i + 1}) {c}" for i, c in enumerate(choices)]
    tail = (
        "\n\nAnswer choices:\n"
        + "\n".join(lines)
        + f"\n\nReply with exactly one integer from 1 to {len(choices)} on the first line."
    )
    return stem + tail, sorted(correct)


def _arc_prompt(doc: Dict[str, Any]) -> Tuple[str, int]:
    q = doc["question"].strip()
    texts = doc["choices"]["text"]
    labels = doc["choices"]["label"]
    key = doc["answerKey"]
    gold = labels.index(key) + 1
    lines = [f"{i + 1}) {t}" for i, t in enumerate(texts)]
    stem = f"Question: {q}\nAnswer:\n\nChoices:\n" + "\n".join(lines)
    stem += f"\n\nReply with exactly one integer from 1 to {len(texts)} on the first line."
    return stem, gold


def _hellaswag_prompt(doc: Dict[str, Any]) -> Tuple[str, int]:
    q = doc["query"].strip()
    choices = doc["choices"]
    if "gold" in doc:
        gold = int(doc["gold"]) + 1
    elif "label" in doc:
        gold = int(str(doc["label"]).strip()) + 1
    else:
        raise ValueError("hellaswag doc missing gold/label")
    lines = [f"{i + 1}) {c}" for i, c in enumerate(choices)]
    stem = q + "\n\nContinuations:\n" + "\n".join(lines)
    stem += f"\n\nReply with exactly one integer from 1 to {len(choices)} on the first line."
    return stem, gold


def detect_builder(doc: Dict[str, Any]):
    if "mc2_targets" in doc:
        return "truthfulqa_mc2"
    if "answerKey" in doc and "choices" in doc and isinstance(doc.get("choices"), dict):
        return "arc"
    if "query" in doc and "choices" in doc:
        return "hellaswag"
    raise ValueError("unrecognized doc schema for replay")


def build_prompt(doc: Dict[str, Any]) -> Tuple[str, Any]:
    kind = detect_builder(doc)
    if kind == "truthfulqa_mc2":
        return _truthfulqa_mc2_prompt(doc)
    if kind == "arc":
        p, g = _arc_prompt(doc)
        return p, g
    p, g = _hellaswag_prompt(doc)
    return p, g


def parse_first_int(text: str) -> Optional[int]:
    for line in text.splitlines():
        m = re.search(r"\b([1-9][0-9]?)\b", line.strip())
        if m:
            return int(m.group(1))
    return None


def parse_cos_verbose(raw: str) -> Tuple[Optional[float], str]:
    sig_m = re.search(r"σ_combined=([0-9.]+)", raw) or re.search(
        r"\[σ=([0-9.]+)", raw
    )
    act_m = re.search(r"action=([A-Z_]+)", raw)
    if not act_m:
        act_m = re.search(r"\[σ=[0-9.]+\s*\|\s*([A-Za-z_]+)", raw)
    sigma = float(sig_m.group(1)) if sig_m else None
    action = act_m.group(1).upper() if act_m else "UNKNOWN"
    if action == "UNKNOWN" and "ABSTAIN" in raw.upper():
        action = "ABSTAIN"
    return sigma, action


def run_cos_chat(cos_bin: Path, prompt: str, timeout_s: int) -> str:
    env = os.environ.copy()
    env.setdefault("COS_BITNET_SERVER", "1")
    env.setdefault("COS_BITNET_SERVER_EXTERNAL", "1")
    env.setdefault("COS_BITNET_SERVER_PORT", "11434")
    env.setdefault("COS_BITNET_CHAT_MODEL", "gemma3:4b")
    env.setdefault("COS_ENGRAM_DISABLE", "1")
    cmd = [
        str(cos_bin),
        "chat",
        "--once",
        "--prompt",
        prompt,
        "--multi-sigma",
        "--semantic-sigma",
        "--verbose",
        "--no-coherence",
        "--no-transcript",
    ]
    try:
        p = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout_s,
            env=env,
        )
        return (p.stdout or "") + (p.stderr or "")
    except subprocess.TimeoutExpired:
        return "CHAT_TIMEOUT action=UNKNOWN σ_combined=1.0"


def is_correct(gold: Any, picked: Optional[int]) -> bool:
    if picked is None:
        return False
    if isinstance(gold, set):
        return picked in gold
    return picked == int(gold)


def load_harness_acc(results_path: Optional[Path]) -> Optional[float]:
    if not results_path or not results_path.is_file():
        return None
    try:
        data = json.loads(results_path.read_text(encoding="utf-8"))
        r = data.get("results", data)
        for k, v in r.items():
            if not isinstance(v, dict):
                continue
            if "acc,none" in v:
                return float(v["acc,none"])
            if "acc" in v and isinstance(v["acc"], (int, float)):
                return float(v["acc"])
    except (OSError, json.JSONDecodeError, TypeError, ValueError):
        return None
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", required=True, help="Glob or path to samples_*.jsonl")
    ap.add_argument("--results-json", default="", help="lm-eval aggregated results JSON")
    ap.add_argument("--out-csv", required=True)
    ap.add_argument("--cos-bin", default="./cos")
    ap.add_argument("--limit", type=int, default=0, help="max rows (0 = all)")
    ap.add_argument("--timeout", type=int, default=300)
    args = ap.parse_args()

    paths = sorted(glob.glob(args.samples))
    if not paths:
        print("error: no files match --samples", file=sys.stderr)
        return 2

    cos_bin = Path(args.cos_bin).expanduser()
    if not cos_bin.is_file():
        cos_bin = Path.cwd() / "cos"
    if not cos_bin.is_file():
        print("error: cos binary not found:", args.cos_bin, file=sys.stderr)
        return 2
    cos_bin = cos_bin.resolve()

    results_path = Path(args.results_json) if args.results_json else None
    if args.results_json and "*" in args.results_json:
        cand = sorted(glob.glob(args.results_json))
        results_path = Path(cand[-1]) if cand else None

    rows_out: List[Dict[str, Any]] = []
    n = 0
    for path in paths:
        with open(path, encoding="utf-8") as f:
            for line in f:
                if args.limit and n >= args.limit:
                    break
                line = line.strip()
                if not line:
                    continue
                row = json.loads(line)
                doc = row["doc"]
                prompt, gold = build_prompt(doc)
                raw = run_cos_chat(cos_bin, prompt, args.timeout)
                picked = parse_first_int(raw)
                sigma, action = parse_cos_verbose(raw)
                ok = is_correct(gold, picked)
                rows_out.append(
                    {
                        "doc_id": row.get("doc_id", n),
                        "picked": picked if picked is not None else "",
                        "gold": gold if not isinstance(gold, set) else ",".join(str(x) for x in sorted(gold)),
                        "correct": int(ok),
                        "sigma": sigma if sigma is not None else "",
                        "action": action,
                    }
                )
                n += 1
        if args.limit and n >= args.limit:
            break

    out_csv = Path(args.out_csv)
    out_csv.parent.mkdir(parents=True, exist_ok=True)
    with open(out_csv, "w", newline="", encoding="utf-8") as fc:
        w = csv.DictWriter(
            fc,
            fieldnames=["doc_id", "picked", "gold", "correct", "sigma", "action"],
        )
        w.writeheader()
        w.writerows(rows_out)

    total = len(rows_out)
    acc_all = sum(r["correct"] for r in rows_out) / total if total else 0.0
    acc_rows = [r for r in rows_out if r["action"] == "ACCEPT"]
    cov = len(acc_rows) / total if total else 0.0
    acc_acc = sum(r["correct"] for r in acc_rows) / len(acc_rows) if acc_rows else 0.0
    sigmas = [float(r["sigma"]) for r in acc_rows if r["sigma"] != ""]
    s_mean = sum(sigmas) / len(sigmas) if sigmas else 0.0

    harness = load_harness_acc(results_path)
    print("samples_rows", total)
    print("harness_acc_official", harness if harness is not None else "n/a")
    print("cos_generative_acc_all", f"{acc_all:.4f}")
    print("cos_generative_acc_accepted", f"{acc_acc:.4f}")
    print("cos_coverage_accept", f"{cov:.4f}")
    print("cos_sigma_mean_accepted", f"{s_mean:.4f}")
    print("wrote_csv", str(out_csv))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
