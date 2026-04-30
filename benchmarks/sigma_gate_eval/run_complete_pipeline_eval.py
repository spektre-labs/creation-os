#!/usr/bin/env python3
"""
Evaluate **SigmaGateComplete**: precheck → streaming (optional) → post gate (optional).

Token savings: full ``max_new_tokens`` when precheck abstains; partial when streaming halts.
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
    ap.add_argument("--tau-skip", type=float, default=0.80)
    ap.add_argument("--precheck-mode", choices=("entropy", "pooled", "head"), default="entropy")
    ap.add_argument("--layer-range", choices=("early", "middle", "late"), default="middle")
    ap.add_argument("--tau-halt", type=float, default=0.95)
    ap.add_argument("--window-size", type=int, default=8)
    ap.add_argument("--max-new-tokens", type=int, default=100)
    ap.add_argument("--holdout-limit", type=int, default=30)
    ap.add_argument("--no-prism", action="store_true")
    ap.add_argument("--post-probe", type=str, default="", help="path to sigma_gate_lsd.pkl for SigmaGate post-check")
    args = ap.parse_args()

    repo = _repo_root()
    sys.path.insert(0, str(repo / "python"))
    sys.path.insert(0, str(_eval_dir()))

    from transformers import AutoModelForCausalLM, AutoTokenizer

    from cos.sigma_gate import SigmaGate
    from cos.sigma_gate_complete import SigmaGateComplete
    from cos.sigma_gate_precheck import SigmaPrecheck
    from cos.sigma_streaming import StreamingSigmaGate

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
        layer_range=str(args.layer_range),
    )
    streaming = StreamingSigmaGate(
        probe_path=None,
        tau_halt=float(args.tau_halt),
        window_size=int(args.window_size),
    )
    post_gate = None
    if str(args.post_probe).strip():
        p = (repo / str(args.post_probe).strip()).resolve() if not Path(args.post_probe).is_absolute() else Path(args.post_probe)
        if p.is_file():
            post_gate = SigmaGate(str(p))

    pipeline = SigmaGateComplete(precheck=precheck, streaming=streaming, post_gate=post_gate)

    csv_path = repo / "benchmarks" / "sigma_gate_lsd" / "splits" / "holdout.csv"
    holdout: List[Dict[str, str]] = []
    with csv_path.open(newline="", encoding="utf-8") as fp:
        holdout = list(csv.DictReader(fp))
    holdout = holdout[: max(1, int(args.holdout_limit))]

    total_saved = 0
    pre_skips = 0
    stream_halts = 0

    for i, row in enumerate(holdout):
        q = (row.get("question") or "").strip()
        if not q:
            continue
        question = prism_wrap_question(q) if use_prism else q
        result: Dict[str, Any] = pipeline.process(
            model,
            tok,
            question,
            max_new_tokens=int(args.max_new_tokens),
        )
        total_saved += int(result.get("tokens_saved", 0))
        if result.get("reason") == "pre_generation_risk":
            pre_skips += 1
        elif result.get("reason") == "streaming_halt":
            stream_halts += 1
        print(
            f"[{i+1}/{len(holdout)}] decision={result.get('decision')} "
            f"toks={result.get('tokens_generated', 0)} saved={result.get('tokens_saved', 0)} "
            f"layers={result.get('layers_passed')}"
        )

    n = len(holdout)
    summary = {
        "generator_model": args.model,
        "precheck_mode": args.precheck_mode,
        "tau_skip": float(args.tau_skip),
        "tau_halt": float(args.tau_halt),
        "window_size": int(args.window_size),
        "n_rows": n,
        "precheck_skips": pre_skips,
        "streaming_halts": stream_halts,
        "tokens_saved_sum": total_saved,
        "avg_saved_per_row": float(total_saved) / max(1, n),
    }
    print("\n" + "=" * 50)
    print("  Complete sigma-gate pipeline (lab harness)")
    print("=" * 50)
    print(f"  Precheck skips:   {pre_skips}/{n}")
    print(f"  Streaming halts: {stream_halts}/{n}")
    print(f"  Tokens saved:    {total_saved}")
    print(f"  Avg saved/row:   {total_saved / max(1, n):.1f}")
    print("=" * 50)

    out_dir = repo / "benchmarks" / "sigma_gate_eval" / "results_complete"
    out_dir.mkdir(parents=True, exist_ok=True)
    (out_dir / "complete_pipeline_summary.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
