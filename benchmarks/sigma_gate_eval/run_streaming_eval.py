#!/usr/bin/env python3
"""
Sweep ``tau_halt`` for ``StreamingSigmaGate``: halts, token counts, and mean σ trajectory.

This harness is **not** a reproduction of arXiv:2509.03531 metrics — validate thresholds locally.
"""
from __future__ import annotations

import argparse
import csv
import json
import sys
from pathlib import Path
from typing import Any, Dict, List


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _eval_dir() -> Path:
    return Path(__file__).resolve().parent


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--model", default="gpt2")
    ap.add_argument(
        "--taus",
        type=str,
        default="0.95,0.98",
        help="comma-separated tau_halt values (sliding-window halt on normalized entropy)",
    )
    ap.add_argument("--window-size", type=int, default=8)
    ap.add_argument("--layer-idx", type=int, default=-3)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--holdout-limit", type=int, default=20)
    ap.add_argument("--no-prism", action="store_true")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_streaming import StreamingSigmaGate

    try:
        from semantic_labeling import prism_wrap_question
    except ImportError as e:
        print("semantic_labeling:", e, file=sys.stderr)
        sys.exit(3)

    use_prism = not bool(args.no_prism)
    taus = [float(x.strip()) for x in str(args.taus).split(",") if x.strip()]

    tok = AutoTokenizer.from_pretrained(args.model)
    if tok.pad_token is None:
        tok.pad_token = tok.eos_token
    model = AutoModelForCausalLM.from_pretrained(args.model)
    model.eval()

    csv_path = repo / "benchmarks" / "sigma_gate_lsd" / "splits" / "holdout.csv"
    holdout: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        holdout = list(csv.DictReader(fp))
    holdout = holdout[: max(1, int(args.holdout_limit))]

    summary: Dict[str, Any] = {
        "generator_model": args.model,
        "window_size": int(args.window_size),
        "layer_idx": int(args.layer_idx),
        "max_new_tokens": int(args.max_new_tokens),
        "by_tau": {},
    }

    for tau in taus:
        gate = StreamingSigmaGate(
            probe_path=None,
            tau_halt=float(tau),
            window_size=int(args.window_size),
            layer_idx=int(args.layer_idx),
        )
        total_saved = 0
        halts = 0
        toks = 0
        for i, row in enumerate(holdout):
            q = (row.get("question") or "").strip()
            if not q:
                continue
            prompt = prism_wrap_question(q) if use_prism else q
            _response, meta = gate.generate_with_gate(
                model,
                tok,
                prompt,
                max_new_tokens=int(args.max_new_tokens),
            )
            toks += int(meta["tokens_generated"])
            if meta["halted"]:
                halts += 1
                total_saved += int(meta["tokens_saved"])
            print(
                f"[tau={tau} {i+1}/{len(holdout)}] toks={meta['tokens_generated']} "
                f"halted={meta['halted']} saved={meta['tokens_saved']} "
                f"mean_sigma={meta['mean_sigma']:.4f} max={meta['max_sigma']:.4f}"
            )
        halt_rate = float(halts) / max(1, len(holdout))
        mean_toks = float(toks) / max(1, len(holdout))
        print(
            f"\ntau={tau}: halts={halts}/{len(holdout)} halt_rate={halt_rate:.1%} "
            f"tokens_saved_sum={total_saved} mean_toks={mean_toks:.1f}\n"
        )
        summary["by_tau"][str(tau)] = {
            "halts": halts,
            "n": len(holdout),
            "halt_rate": halt_rate,
            "tokens_saved_sum": total_saved,
            "mean_tokens_per_row": mean_toks,
        }
        if halt_rate > 0.5:
            print(
                f"NOTE: halt_rate={halt_rate:.1%} still > 50% for tau={tau}; "
                "try --taus 0.98 or higher, or increase --window-size.",
                file=sys.stderr,
            )

    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_streaming"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "streaming_eval_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
