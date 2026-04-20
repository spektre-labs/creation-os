# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Cost-measurement driver for the σ-gated hybrid pipeline.

Replays a question set through the pipeline and reports

    "Creation OS saves X% of API costs while maintaining Y% accuracy."

The headline number.  Not AURCC.  Not Bonferroni.  Euros saved + accuracy
maintained.

Input format
------------

A JSONL file, one question per line::

    {"id": "q001", "prompt": "What is 2+2?", "reference": "4",
     "local_correct": true, "api_correct": true}

    * ``id``            arbitrary identifier (str)
    * ``prompt``        question text (str)
    * ``reference``     expected answer snippet (str, optional)
    * ``local_correct`` whether the small model, run alone, is correct
                        on this question (bool)
    * ``api_correct``   whether the big model is correct (bool)

The script does NOT call any LLM — it only needs the correctness labels
plus the per-question σ_peak (from a σ-sidecar OR sampled from the stub
backend's deterministic trace).  This keeps the cost calculation reproducible
on any host.

For real measurements the caller supplies a JSONL with pre-computed
``sigma_peak`` values.  For demos / CI we use the StubBackend to generate
deterministic σ_peak values on the fly.

Pricing
-------

Two prices (per-request, not per-token; keep it simple):
    ``--eur-local``   cost of one small-model local call (default 0.0005€)
    ``--eur-api``     cost of one big-model API call       (default 0.02€)

These defaults assume DeepSeek-V3 tier pricing (≈$0.28/M in + $1.1/M out
on DeepSeek) and a 200-token average reply — your mileage will vary; the
output is a linear function of both so users can re-compute.

Output
------

One Markdown report to stdout + one machine-readable JSON to the ``--out``
path.  Fields::

    {
      "n_total":         int,
      "n_escalated":     int,
      "n_local":         int,
      "eur_local":       float,
      "eur_api":         float,
      "cost_api_only":   float,    # n_total * eur_api
      "cost_hybrid":     float,    # n_local * eur_local + n_esc * (eur_local + eur_api)
      "savings_frac":    float,    # ∈ [0, 1]
      "accuracy_api":    float,    # n_api_correct / n_total
      "accuracy_local":  float,    # n_local_correct / n_total
      "accuracy_hybrid": float,    # P5 headline: hybrid accuracy
      "tau_escalate":    float
    }

Usage
-----

    # deterministic demo with the stub backend (CI default):
    python -m sigma_pipeline.cost_measure --fixture demo --tau-escalate 0.75

    # real benchmark replay (user-supplied sigma_peak per row):
    python -m sigma_pipeline.cost_measure --input ./bench.jsonl \\
        --eur-local 0.0005 --eur-api 0.02 --tau-escalate 0.75 \\
        --out ./results.json
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import math
import pathlib
import sys
from typing import Dict, Iterator, List, Optional, Tuple

if __package__ in (None, ""):
    _here = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(_here))
    from sigma_pipeline.backends import StubBackend           # noqa: E402
    from sigma_pipeline.speculative import (                   # noqa: E402
        Route, cost_savings, peak_update, route,
    )
else:
    from .backends import StubBackend
    from .speculative import Route, cost_savings, peak_update, route


# Built-in demo fixture — deterministic, for CI.  10 rows: 7 local-correct,
# 3 api-correct-only.  σ_peak values chosen so ~30% cross τ=0.75.
DEMO_FIXTURE: List[Dict[str, object]] = [
    {"id": "q01", "sigma_peak": 0.15, "local_correct": True,  "api_correct": True},
    {"id": "q02", "sigma_peak": 0.22, "local_correct": True,  "api_correct": True},
    {"id": "q03", "sigma_peak": 0.30, "local_correct": True,  "api_correct": True},
    {"id": "q04", "sigma_peak": 0.42, "local_correct": True,  "api_correct": True},
    {"id": "q05", "sigma_peak": 0.55, "local_correct": True,  "api_correct": True},
    {"id": "q06", "sigma_peak": 0.68, "local_correct": True,  "api_correct": True},
    {"id": "q07", "sigma_peak": 0.72, "local_correct": True,  "api_correct": True},
    {"id": "q08", "sigma_peak": 0.82, "local_correct": False, "api_correct": True},
    {"id": "q09", "sigma_peak": 0.88, "local_correct": False, "api_correct": True},
    {"id": "q10", "sigma_peak": 0.95, "local_correct": False, "api_correct": True},
]


@dataclasses.dataclass
class RowOutcome:
    id_: str
    sigma_peak: float
    route_: Route
    correct: bool           # hybrid correctness after routing
    local_correct: bool
    api_correct: bool


def _load_rows(path: pathlib.Path) -> List[Dict[str, object]]:
    rows: List[Dict[str, object]] = []
    with path.open("r", encoding="utf-8") as fh:
        for lineno, line in enumerate(fh, 1):
            line = line.strip()
            if not line:
                continue
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError as e:
                raise SystemExit(f"{path}:{lineno}: invalid JSONL: {e}") from None
    return rows


def _stub_sigma_peak(prompt: str, max_tokens: int = 32) -> float:
    """Deterministic σ_peak from the stub backend for a given prompt.

    Used when the input fixture has no sigma_peak field.  Drives the same
    StubBackend the chat demo uses so σ values agree across tools.
    """
    sb = StubBackend(max_steps=max_tokens)
    peak = 0.0
    acc = ""
    for _ in range(max_tokens):
        ts = sb.step(prompt, acc)
        if ts.eos and ts.token_text == "":
            break
        acc += ts.token_text
        peak = peak_update(peak, ts.sigma_max)
        if ts.eos:
            break
    sb.close()
    return peak


def _compute(rows: List[Dict[str, object]], tau_escalate: float,
             eur_local: float, eur_api: float,
             fill_sigma: bool) -> Tuple[List[RowOutcome], Dict[str, object]]:
    outcomes: List[RowOutcome] = []
    n_escalated = 0
    n_correct_hybrid = 0
    n_correct_local = 0
    n_correct_api   = 0

    for r in rows:
        rid = str(r.get("id", ""))
        lc = bool(r.get("local_correct", False))
        ac = bool(r.get("api_correct",   False))
        if "sigma_peak" in r:
            sp = float(r["sigma_peak"])
        elif fill_sigma:
            sp = _stub_sigma_peak(str(r.get("prompt", rid)))
        else:
            raise SystemExit(
                f"row {rid!r} has no sigma_peak and --fill-sigma not set"
            )
        rt = route(sp, tau_escalate)
        # Hybrid correctness: LOCAL → small model; ESCALATE → big model.
        correct = ac if rt is Route.ESCALATE else lc
        if rt is Route.ESCALATE:
            n_escalated += 1
        n_correct_hybrid += int(correct)
        n_correct_local  += int(lc)
        n_correct_api    += int(ac)
        outcomes.append(RowOutcome(
            id_=rid, sigma_peak=sp, route_=rt, correct=correct,
            local_correct=lc, api_correct=ac,
        ))

    n_total = len(outcomes)
    n_local = n_total - n_escalated
    cost_api_only  = n_total * eur_api
    cost_hybrid    = n_local * eur_local + n_escalated * (eur_local + eur_api)
    savings = cost_savings(n_total, n_escalated, eur_local, eur_api)
    accuracy_api    = n_correct_api    / n_total if n_total else 0.0
    accuracy_local  = n_correct_local  / n_total if n_total else 0.0
    accuracy_hybrid = n_correct_hybrid / n_total if n_total else 0.0

    summary = {
        "n_total":         n_total,
        "n_escalated":     n_escalated,
        "n_local":         n_local,
        "eur_local":       eur_local,
        "eur_api":         eur_api,
        "cost_api_only":   round(cost_api_only, 6),
        "cost_hybrid":     round(cost_hybrid,   6),
        "savings_frac":    round(savings,       6),
        "accuracy_api":    round(accuracy_api,    6),
        "accuracy_local":  round(accuracy_local,  6),
        "accuracy_hybrid": round(accuracy_hybrid, 6),
        "tau_escalate":    tau_escalate,
        # Derived "accuracy retained" metric: how much of the API-only
        # accuracy is preserved by the hybrid?
        "accuracy_retained": round(
            accuracy_hybrid / accuracy_api if accuracy_api > 0 else 0.0,
            6,
        ),
    }
    return outcomes, summary


def _render_markdown(summary: Dict[str, object],
                     outcomes: List[RowOutcome]) -> str:
    md: List[str] = []
    md.append("# σ-gated hybrid cost measurement")
    md.append("")
    md.append(f"- **n_total:** {summary['n_total']}")
    md.append(f"- **τ_escalate:** {summary['tau_escalate']:.3f}")
    md.append(f"- **escalations:** {summary['n_escalated']} "
              f"/ {summary['n_total']} "
              f"({summary['n_escalated'] / max(1, summary['n_total']):.1%})")
    md.append("")
    md.append("## Cost")
    md.append("")
    md.append(f"| strategy | calls (local) | calls (API) | cost |")
    md.append(f"|----------|--------------:|------------:|-----:|")
    md.append(f"| API only     | 0                | {summary['n_total']}   | "
              f"{summary['cost_api_only']:.4f}€ |")
    md.append(f"| local only   | {summary['n_total']}   | 0                | "
              f"{summary['n_total'] * summary['eur_local']:.4f}€ |")
    md.append(f"| σ-gated hybrid | {summary['n_total']}   | {summary['n_escalated']} "
              f"| {summary['cost_hybrid']:.4f}€ |")
    md.append("")
    md.append(f"**Savings (hybrid vs API-only): "
              f"{summary['savings_frac']:.1%}**")
    md.append("")
    md.append("## Accuracy")
    md.append("")
    md.append(f"| strategy | accuracy |")
    md.append(f"|----------|---------:|")
    md.append(f"| API only       | {summary['accuracy_api']:.1%} |")
    md.append(f"| local only     | {summary['accuracy_local']:.1%} |")
    md.append(f"| σ-gated hybrid | {summary['accuracy_hybrid']:.1%} |")
    md.append("")
    md.append(f"**Accuracy retained vs API-only: "
              f"{summary['accuracy_retained']:.1%}**")
    md.append("")
    md.append(
        f"### Headline — "
        f"Creation OS saves **{summary['savings_frac']:.0%}** of API costs "
        f"while maintaining **{summary['accuracy_retained']:.0%}** "
        f"of API-only accuracy."
    )
    md.append("")
    md.append("## Row-level trace")
    md.append("")
    md.append("| id | σ_peak | route | hybrid correct | local correct | api correct |")
    md.append("|----|-------:|-------|:--------------:|:-------------:|:-----------:|")
    for o in outcomes:
        md.append(
            f"| {o.id_} | {o.sigma_peak:.3f} | {o.route_.name} | "
            f"{'yes' if o.correct else 'no'} | "
            f"{'yes' if o.local_correct else 'no'} | "
            f"{'yes' if o.api_correct else 'no'} |"
        )
    md.append("")
    return "\n".join(md)


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="σ-gated hybrid cost measurement")
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--input", type=pathlib.Path,
                     help="JSONL with id / sigma_peak / local_correct / api_correct")
    src.add_argument("--fixture", choices=("demo",),
                     help="use the built-in deterministic demo fixture")
    ap.add_argument("--tau-escalate", type=float, default=0.75)
    ap.add_argument("--eur-local", type=float, default=0.0005)
    ap.add_argument("--eur-api",   type=float, default=0.02)
    ap.add_argument("--fill-sigma", action="store_true",
                    help="if a row has no sigma_peak, synthesize one from "
                         "the StubBackend's deterministic σ trace")
    ap.add_argument("--out", type=pathlib.Path, default=None,
                    help="write machine-readable JSON summary here")
    ap.add_argument("--markdown", type=pathlib.Path, default=None,
                    help="also write the Markdown report here "
                         "(default: print to stdout)")
    args = ap.parse_args(argv)

    if args.fixture == "demo":
        rows: List[Dict[str, object]] = [dict(r) for r in DEMO_FIXTURE]
    else:
        rows = _load_rows(args.input)
        if not rows:
            raise SystemExit(f"{args.input}: no rows")

    outcomes, summary = _compute(
        rows, args.tau_escalate,
        args.eur_local, args.eur_api,
        args.fill_sigma,
    )

    md = _render_markdown(summary, outcomes)
    if args.markdown is not None:
        args.markdown.write_text(md, encoding="utf-8")
    else:
        print(md)

    if args.out is not None:
        args.out.write_text(json.dumps(summary, sort_keys=True, indent=2)
                            + "\n", encoding="utf-8")

    # Also emit a one-line JSON on stderr so callers can grep without
    # parsing the Markdown.
    print(json.dumps(summary, sort_keys=True), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
