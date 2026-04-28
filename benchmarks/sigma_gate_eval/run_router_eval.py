#!/usr/bin/env python3
"""
Evaluate ``SigmaRouter`` tier distribution on TruthfulQA holdout rows.

Interpreting ``fast_pct`` etc. requires a calibrated ``SigmaPrecheck`` and thresholds —
this script is a harness only (``docs/CLAIM_DISCIPLINE.md``).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _eval_dir() -> Path:
    return Path(__file__).resolve().parent


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument("--tau-skip", type=float, default=0.90, help="passed to SigmaPrecheck only")
    ap.add_argument("--tau-fast", type=float, default=0.3, help="sigma_pre <= this → FAST (band router)")
    ap.add_argument("--tau-verify", type=float, default=0.6, help="sigma_pre <= this → VERIFY after FAST band")
    ap.add_argument("--tau-escalate", type=float, default=0.85, help="sigma_pre <= this → RAG/VERIFY; above → ESCALATE/ABSTAIN")
    ap.add_argument("--precheck-mode", choices=("entropy", "pooled", "head"), default="entropy")
    ap.add_argument("--holdout-limit", type=int, default=0, help="0 = all rows")
    ap.add_argument("--max-new-tokens", type=int, default=80)
    ap.add_argument("--no-prism", action="store_true")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    import csv

    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate_precheck import SigmaPrecheck
    from cos.sigma_router import SigmaRouter

    try:
        from semantic_labeling import prism_wrap_question
    except ImportError as e:
        print("semantic_labeling:", e, file=sys.stderr)
        sys.exit(3)

    use_prism = not bool(args.no_prism)

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model, output_hidden_states=True)
    model.eval()

    precheck = SigmaPrecheck(
        tau_skip=float(args.tau_skip),
        mode=str(args.precheck_mode),
    )

    router = SigmaRouter(
        precheck=precheck,
        post_gate=None,
        local_model=model,
        local_tokenizer=tok,
        route_by_precheck_thresholds=True,
        tau_fast=float(args.tau_fast),
        tau_verify=float(args.tau_verify),
        tau_escalate=float(args.tau_escalate),
    )

    csv_path = repo / "benchmarks" / "sigma_gate_lsd" / "splits" / "holdout.csv"
    rows: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        rows = list(csv.DictReader(fp))
    if int(args.holdout_limit) > 0:
        rows = rows[: int(args.holdout_limit)]

    for i, row in enumerate(rows):
        q = (row.get("question") or "").strip()
        if not q:
            continue
        question = prism_wrap_question(q) if use_prism else q
        result = router.route(question, max_new_tokens=int(args.max_new_tokens))
        sp = result.sigma_post
        sp_s = f"{sp:.3f}" if sp is not None else "-"
        print(f"[{i+1}/{len(rows)}] sigma_pre={result.sigma_pre:.3f} sigma_post={sp_s} -> {result.route}")

    stats = router.get_stats()
    print("\n" + "=" * 50)
    print("  sigma-router distribution (this run)")
    print("=" * 50)
    print(f"  FAST:     {stats['fast_pct']:.0%}")
    print(f"  VERIFY:   {stats['verify_pct']:.0%}")
    print(f"  RAG:      {stats['rag_pct']:.0%}")
    print(f"  ESCALATE: {stats['escalate_pct']:.0%}")
    print(f"  ABSTAIN:  {stats['abstain_pct']:.0%}")
    print(f"  Avg illustrative cost: ${stats['avg_cost']:.4f}/query")
    print("=" * 50)

    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_router"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "router_summary.json").write_text(json.dumps(stats, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
