# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Creation OS **cos** CLI (minimal). Heavy teacher/student wiring stays in harness scripts;
this entrypoint performs JSONL I/O and optional mock distillation for CI smoke tests.

Run: ``PYTHONPATH=python python -m cos …`` or ``./scripts/cos …`` from the repo root.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, Iterator, List, Optional


def _iter_prompts_jsonl(path: Path) -> Iterator[str]:
    with path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except json.JSONDecodeError:
                continue
            if isinstance(obj, dict):
                p = obj.get("prompt") or obj.get("text") or obj.get("instruction")
                if isinstance(p, str) and p.strip():
                    yield p.strip()


def _cmd_distill_generate(args: argparse.Namespace) -> int:
    from cos.sigma_distill import SigmaDistill

    prompts_path = Path(args.prompts)
    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if args.mock:

        class _MockTeacher:
            def generate(self, prompt: str) -> str:
                return f"mock_response::{prompt[:256]}"

        class _ConstGate:
            def compute_sigma(self, _teacher: Any, _prompt: str, _output: str) -> float:
                return float(args.mock_sigma)

        teacher: Any = _MockTeacher()
        gate: Any = _ConstGate()
    else:
        print(
            "cos distill generate: non-mock mode requires a wired SigmaDistill in a harness "
            "(install transformers, set CREATION_* env, etc.). Use --mock for JSONL smoke.",
            file=sys.stderr,
        )
        return 2

    student: Any = object()
    sd = SigmaDistill(teacher=teacher, student=student, gate=gate, k_raw=float(args.k_raw))
    rows = sd.generate_training_data(_iter_prompts_jsonl(prompts_path), limit=int(args.limit))
    with out_path.open("w", encoding="utf-8") as out:
        for row in rows:
            out.write(json.dumps(row, ensure_ascii=False) + "\n")
    print(f"wrote {len(rows)} rows to {out_path}")
    return 0


def _cmd_distill_train(args: argparse.Namespace) -> int:
    data_path = Path(args.data)
    epochs = max(1, int(args.epochs))

    class _CounterStudent:
        def __init__(self) -> None:
            self.n = 0
            self.weight_sum = 0.0

        def train_step(self, prompt: str, response: str, *, weight: float = 1.0) -> float:
            self.n += 1
            self.weight_sum += float(weight)
            return 0.0

    from cos.sigma_distill import SigmaDistill

    rows: List[Dict[str, Any]] = []
    with data_path.open("r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                continue

    stu = _CounterStudent()
    dummy_teacher: Any = object()
    dummy_gate: Any = object()

    sd = SigmaDistill(teacher=dummy_teacher, student=stu, gate=dummy_gate)
    sd.distill(rows, epochs=epochs)
    print(f"train_step calls: {stu.n} total_weight: {stu.weight_sum:.6f}")
    return 0


def _cmd_distill_eval(args: argparse.Namespace) -> int:
    print(
        "cos distill eval: stub — wire benchmarks/sigma_gate_eval or lm-eval harness "
        f"(student={args.student!r} benchmark={args.benchmark!r}).",
        file=sys.stderr,
    )
    return 0


def _cmd_debate(args: argparse.Namespace) -> int:
    from cos.sigma_debate import SigmaDebate

    if not args.mock:
        print(
            "cos debate: use --mock for JSON stdout smoke, or wire HF / vLLM deputies in a harness.",
            file=sys.stderr,
        )
        return 2

    names = [x.strip() for x in str(args.models).split(",") if x.strip()]
    if len(names) < 2:
        print("cos debate: need at least two comma-separated names in --models", file=sys.stderr)
        return 2

    class _ArgDeputy:
        def __init__(self, tag: str) -> None:
            self.tag = tag

        def argue(self, question: str, history: List[Dict[str, Any]]) -> str:
            opp = len(history)
            return f"{self.tag}:{question[:80]}:r{opp}"

    class _SkewGate:
        """Lower σ for the first deputy so mock debates pick a winner."""

        def compute_sigma(self, model: Any, _q: str, _arg: str) -> float:
            if isinstance(model, _ArgDeputy) and model.tag == names[0]:
                return 0.22
            return 0.48

    a = _ArgDeputy(names[0])
    b = _ArgDeputy(names[1])
    sd = SigmaDebate(a, b, _SkewGate())
    text, side, stat = sd.debate(str(args.question), rounds=int(args.rounds))
    print(json.dumps({"winner_side": side, "stat": stat, "text": text}, ensure_ascii=False))
    return 0


def _cmd_self_play(args: argparse.Namespace) -> int:
    from cos.sigma_selfplay import SigmaSelfPlay

    if not args.mock:
        print(
            "cos self-play: use --mock for JSONL smoke, or wire a full model with generate/critique.",
            file=sys.stderr,
        )
        return 2

    class _SelfModel:
        def generate(self, question: str, temperature: float = 0.5) -> str:
            return f"t{temperature:.1f}:{question[:48]}"

        def critique(self, question: str, answer: str) -> str:
            return f"critique_of({answer[:24]})"

    class _Gate:
        def compute_sigma(self, _m: Any, _q: str, text: str) -> float:
            if text.startswith("t0.3"):
                return 0.25
            if text.startswith("critique"):
                return 0.55
            return 0.45

    sp = SigmaSelfPlay(_Gate())
    out_path = Path(args.output) if getattr(args, "output", None) else None
    rows: List[Dict[str, Any]] = []
    prompts_path = Path(args.prompts) if args.prompts else None
    if prompts_path is not None and prompts_path.is_file():
        for pq in _iter_prompts_jsonl(prompts_path):
            ans, sig = sp.play(_SelfModel(), pq)
            rows.append({"prompt": pq, "answer": ans, "sigma": float(sig)})
    else:
        pq = str(args.question or "").strip() or "default prompt"
        ans, sig = sp.play(_SelfModel(), pq)
        rows.append({"prompt": pq, "answer": ans, "sigma": float(sig)})

    if out_path is not None:
        out_path.parent.mkdir(parents=True, exist_ok=True)
        with out_path.open("w", encoding="utf-8") as f:
            for row in rows:
                f.write(json.dumps(row, ensure_ascii=False) + "\n")
        print(f"wrote {len(rows)} rows to {out_path}")
    else:
        print(json.dumps(rows, ensure_ascii=False))
    return 0


def _cmd_proconductor(args: argparse.Namespace) -> int:
    from cos.sigma_proconductor import ProconductorDebate

    _ = args.all_models
    if not args.mock:
        print(
            "cos proconductor: use --mock for stdout JSON, or wire four backends in a harness.",
            file=sys.stderr,
        )
        return 2

    class _Nm:
        def __init__(self, name: str, answer: str) -> None:
            self.name = name
            self._answer = answer

        def generate(self, question: str) -> str:
            _ = question
            return self._answer

    class _PGate:
        def compute_sigma(self, model: Any, _q: str, answer: str) -> float:
            if "consensus" in answer:
                return 0.2
            return 0.75

    models = [
        _Nm("bitnet", "consensus path alpha"),
        _Nm("qwen3", "consensus path alpha"),
        _Nm("gemma3", "consensus path alpha"),
        _Nm("deepseek", "lonely road"),
    ]
    pc = ProconductorDebate(models, _PGate())
    ans, sig = pc.multi_debate(str(args.question))
    print(json.dumps({"answer": ans, "mean_sigma": sig}, ensure_ascii=False))
    return 0


def _cmd_omega(args: argparse.Namespace) -> int:
    from cos.omega import OmegaLoop

    loop = OmegaLoop()
    if getattr(args, "mock", False):
        _ = loop  # reserved: swap mock backends in harness builds
    history = loop.run(str(args.goal), max_turns=int(args.turns))
    if getattr(args, "json", False):
        print(json.dumps(history, ensure_ascii=False))
        return 0
    n_ph = len(history[0]["phases"]) if history else 0
    last = history[-1] if history else {}
    print(
        json.dumps(
            {
                "turns": len(history),
                "phases_per_turn": n_ph,
                "last_continue": bool(last.get("continue", False)),
                "last_watchdog": int(last.get("watchdog", -1)),
            },
            ensure_ascii=False,
        )
    )
    return 0


def main(argv: Optional[List[str]] = None) -> int:
    argv = argv if argv is not None else sys.argv[1:]
    ap = argparse.ArgumentParser(prog="cos", description="Creation OS cos CLI")
    sub = ap.add_subparsers(dest="cmd", required=True)

    distill = sub.add_parser("distill", help="σ-filtered distillation helpers")
    dsub = distill.add_subparsers(dest="distill_cmd", required=True)

    g = dsub.add_parser("generate", help="teacher → σ-filter → JSONL")
    g.add_argument("--teacher", type=str, default="", help="label only unless harness wired")
    g.add_argument("--prompts", type=str, required=True, help="input .jsonl (prompt/text/instruction keys)")
    g.add_argument("--output", type=str, required=True, help="output .jsonl")
    g.add_argument("--limit", type=int, default=10_000)
    g.add_argument("--k-raw", type=float, default=0.95, dest="k_raw")
    g.add_argument("--mock", action="store_true", help="use mock teacher + constant σ gate")
    g.add_argument("--mock-sigma", type=float, default=0.18, help="σ used with --mock (stable ACCEPT band)")
    g.set_defaults(func=_cmd_distill_generate)

    t = dsub.add_parser("train", help="student train_step over clean JSONL")
    t.add_argument("--student", type=str, default="", help="label only (harness wiring)")
    t.add_argument("--data", type=str, required=True, help="clean_train.jsonl from generate")
    t.add_argument("--epochs", type=int, default=3)
    t.set_defaults(func=_cmd_distill_train)

    ev = dsub.add_parser("eval", help="evaluation stub")
    ev.add_argument("--student", type=str, default="")
    ev.add_argument("--benchmark", type=str, default="truthfulqa")
    ev.set_defaults(func=_cmd_distill_eval)

    deb = sub.add_parser("debate", help="σ-scored two-deputy debate (mock or harness)")
    deb.add_argument("--models", type=str, required=True, help='comma-separated labels, e.g. "a,b"')
    deb.add_argument("--question", type=str, required=True)
    deb.add_argument("--rounds", type=int, default=3)
    deb.add_argument("--mock", action="store_true", help="run skewed mock deputies + σ gate")
    deb.set_defaults(func=_cmd_debate)

    sp = sub.add_parser("self-play", help="σ self-play over prompts (mock or harness)")
    sp.add_argument("--model", type=str, default="", help="label only unless harness wired")
    sp.add_argument("--prompts", type=str, default="", help="optional JSONL of prompts")
    sp.add_argument("--question", type=str, default="", help="single prompt if no --prompts")
    sp.add_argument("--output", type=str, default="", help="optional JSONL output path")
    sp.add_argument("--mock", action="store_true")
    sp.set_defaults(func=_cmd_self_play)

    pr = sub.add_parser("proconductor", help="four-deputy σ + exact-text consensus (mock or harness)")
    pr.add_argument("--question", type=str, required=True)
    pr.add_argument("--all-models", action="store_true", dest="all_models", help="reserved for harness wiring")
    pr.add_argument("--mock", action="store_true")
    pr.set_defaults(func=_cmd_proconductor)

    om = sub.add_parser("omega", help="Ω-loop harness (14 σ phases per turn; lab scaffold)")
    om.add_argument("--goal", type=str, required=True, help="task / objective string")
    om.add_argument("--turns", type=int, default=50, help="maximum Ω turns (default 50)")
    om.add_argument("--mock", action="store_true", help="reserved for mock backends in harness wiring")
    om.add_argument("--json", action="store_true", help="print full turn history as JSON")
    om.set_defaults(func=_cmd_omega)

    ns = ap.parse_args(argv)
    fn = getattr(ns, "func", None)
    if fn is None:
        ap.print_help()
        return 2
    return int(fn(ns))


if __name__ == "__main__":
    raise SystemExit(main())
