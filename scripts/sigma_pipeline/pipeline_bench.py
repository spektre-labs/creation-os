#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""End-to-end pipeline benchmark (P9).

Given a JSONL fixture with one ``{"id", "prompt", "truth"}`` per line
AND per-prompt reproducibility labels ``local_correct`` /
``api_correct`` (treated as oracles since we don't run MMLU here), the
bench:

  1. Warms a single ``Orchestrator`` (engram + TTT persist across the
     full run).
  2. Calls ``orchestrate(prompt)`` for every row; tallies per-row
     origin (engram / local / api), rounds, ttt_updates, ttt_resets,
     elapsed_ms, eur_cost.
  3. Uses the fixture's ``local_correct`` / ``api_correct`` labels to
     compute the hybrid's effective accuracy:
       · origin == "engram"  → accuracy = api_correct (the stored
                               answer came from whichever path solved
                               it originally; we credit the cache).
       · origin == "local"   → accuracy = local_correct.
       · origin == "api"     → accuracy = api_correct.
  4. Emits a Markdown report + machine-readable JSON summary with the
     five headline numbers the user asked for:
       · accuracy_api     (baseline: always-API)
       · accuracy_local   (small-model-only)
       · accuracy_hybrid  (Creation OS pipeline)
       · local_share      (fraction of queries served without API,
                           engram + local combined)
       · savings_frac     (€ saved vs always-API)

Everything is deterministic over the built-in DEMO_FIXTURE so the
smoke test can pin exact numbers.  Users can pass ``--input`` to point
at a larger fixture.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import math
import pathlib
import sys
import time
from typing import Any, Dict, List, Optional

if __package__ in (None, ""):
    _here = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(_here))
    from sigma_pipeline.backends import StubBackend                 # noqa: E402
    from sigma_pipeline.orchestrator import (                       # noqa: E402
        Orchestrator, OrchestratorConfig,
    )
else:
    from .backends import StubBackend
    from .orchestrator import Orchestrator, OrchestratorConfig


# Deterministic 20-row demo fixture.  Mix of (prompts the stub answers
# confidently, prompts where σ spikes).  The ``local_correct`` /
# ``api_correct`` labels are the oracle for this fixture — in a real
# run they'd come from a MMLU harness.
DEMO_FIXTURE: List[Dict[str, Any]] = [
    # easy: stub's σ on "What is 2+2?" goes mid; we mark local correct.
    *[{"id": f"e{i}", "prompt": "What is 2+2?",
       "truth": "4", "local_correct": True, "api_correct": True}
      for i in range(8)],
    # hard: Fermat — stub still spikes; local wrong, api right.
    *[{"id": f"h{i}", "prompt": "Prove Fermat's last theorem.",
       "truth": "(hard)", "local_correct": False, "api_correct": True}
      for i in range(6)],
    # code: another spiking prompt; api only.
    *[{"id": f"c{i}", "prompt": f"Write a Turing-complete lambda {i}.",
       "truth": "(hard)", "local_correct": False, "api_correct": True}
      for i in range(6)],
]


@dataclasses.dataclass
class BenchRow:
    id: str
    origin: str
    rounds: int
    engram_hit: bool
    ttt_updates: int
    ttt_resets: int
    escalated: bool
    abstained: bool
    sigma_final: float
    elapsed_ms: float
    eur_cost: float
    correct: bool


@dataclasses.dataclass
class BenchSummary:
    n: int
    n_engram: int
    n_local: int
    n_api: int
    n_escalated: int
    n_abstained: int
    rounds_total: int
    ttt_updates_total: int
    ttt_resets_total: int
    elapsed_ms_total: float
    eur_cost_hybrid: float
    eur_cost_api_only: float
    savings_frac: float
    accuracy_api: float
    accuracy_local: float
    accuracy_hybrid: float
    accuracy_retained: float
    local_share: float


def _load(fixture_path: Optional[pathlib.Path]
          ) -> List[Dict[str, Any]]:
    if fixture_path is None:
        return list(DEMO_FIXTURE)
    rows: List[Dict[str, Any]] = []
    with open(fixture_path, "r", encoding="utf-8") as fh:
        for ln in fh:
            ln = ln.strip()
            if not ln:
                continue
            rows.append(json.loads(ln))
    return rows


def run_bench(fixture: List[Dict[str, Any]],
              cfg: Optional[OrchestratorConfig] = None,
              log_path: Optional[pathlib.Path] = None,
              ) -> tuple[List[BenchRow], BenchSummary]:
    cfg = cfg or OrchestratorConfig(max_tokens=16)
    # One orchestrator for the whole run so engram warms up.  We give
    # it a fresh StubBackend per call so each row's token counter
    # starts at 0 (otherwise StubBackend's _step carries over and the
    # second identical prompt gets a different σ trace).
    orch: Optional[Orchestrator] = None

    def _orch() -> Orchestrator:
        # Swap in a fresh backend each call; keep engram + TTT.
        nonlocal orch
        if orch is None:
            orch = Orchestrator(StubBackend(max_steps=256), cfg)
        else:
            orch._backend = StubBackend(max_steps=256)
        return orch

    rows: List[BenchRow] = []
    log_fh = None
    if log_path is not None:
        log_path = pathlib.Path(log_path)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        log_fh = open(log_path, "w", encoding="utf-8")
        log_fh.write(json.dumps({
            "kind": "start", "n_rows": len(fixture),
            "cfg": dataclasses.asdict(cfg),
        }) + "\n")

    for r in fixture:
        v = _orch().orchestrate(r["prompt"])
        # Oracle mapping: which correctness label applies?
        if v.origin == "engram":
            correct = bool(r.get("api_correct", False))
        elif v.origin == "api":
            correct = bool(r.get("api_correct", False))
        else:
            correct = bool(r.get("local_correct", False))
        br = BenchRow(
            id=str(r["id"]),
            origin=v.origin,
            rounds=v.rounds,
            engram_hit=v.engram_hit,
            ttt_updates=v.ttt_updates,
            ttt_resets=v.ttt_resets,
            escalated=v.escalated,
            abstained=v.abstained,
            sigma_final=v.sigma_final,
            elapsed_ms=v.elapsed_ms,
            eur_cost=v.eur_cost,
            correct=correct,
        )
        rows.append(br)
        if log_fh:
            log_fh.write(json.dumps(
                {"kind": "row", **dataclasses.asdict(br)},
            ) + "\n")

    n = len(rows)
    n_engram = sum(1 for x in rows if x.origin == "engram")
    n_local  = sum(1 for x in rows if x.origin == "local")
    n_api    = sum(1 for x in rows if x.origin == "api")
    n_esc    = sum(1 for x in rows if x.escalated)
    n_abs    = sum(1 for x in rows if x.abstained)

    acc_hybrid = sum(1 for x in rows if x.correct) / max(n, 1)
    acc_api   = sum(1 for r in fixture if r.get("api_correct"))   / max(n, 1)
    acc_local = sum(1 for r in fixture if r.get("local_correct")) / max(n, 1)
    acc_retained = acc_hybrid / acc_api if acc_api > 0 else 0.0

    eur_hybrid   = sum(x.eur_cost for x in rows)
    eur_api_only = cfg.eur_per_api * n
    savings_frac = 1.0 - (eur_hybrid / eur_api_only) if eur_api_only > 0 else 0.0
    local_share  = (n_engram + n_local) / max(n, 1)

    summary = BenchSummary(
        n=n, n_engram=n_engram, n_local=n_local, n_api=n_api,
        n_escalated=n_esc, n_abstained=n_abs,
        rounds_total=sum(x.rounds for x in rows),
        ttt_updates_total=sum(x.ttt_updates for x in rows),
        ttt_resets_total=sum(x.ttt_resets for x in rows),
        elapsed_ms_total=sum(x.elapsed_ms for x in rows),
        eur_cost_hybrid=eur_hybrid,
        eur_cost_api_only=eur_api_only,
        savings_frac=savings_frac,
        accuracy_api=acc_api,
        accuracy_local=acc_local,
        accuracy_hybrid=acc_hybrid,
        accuracy_retained=acc_retained,
        local_share=local_share,
    )
    if log_fh:
        log_fh.write(json.dumps({
            "kind": "summary", **dataclasses.asdict(summary),
        }) + "\n")
        log_fh.close()
    return rows, summary


def format_markdown(rows: List[BenchRow], s: BenchSummary) -> str:
    md: List[str] = []
    md.append("# Creation OS — end-to-end pipeline benchmark\n")
    md.append(f"- rows: **{s.n}**")
    md.append(f"- engram hits: **{s.n_engram}**  "
              f"({s.n_engram / max(s.n, 1):.0%})")
    md.append(f"- local-only (BitNet accepts): **{s.n_local}**  "
              f"({s.n_local / max(s.n, 1):.0%})")
    md.append(f"- API escalations: **{s.n_api}**  "
              f"({s.n_api / max(s.n, 1):.0%})")
    md.append(f"- σ-reinforce rounds total: {s.rounds_total}")
    md.append(f"- σ-TTT updates total: {s.ttt_updates_total}  "
              f"(resets: {s.ttt_resets_total})")
    md.append(f"- wall-time total: {s.elapsed_ms_total:.1f} ms  "
              f"(mean: {s.elapsed_ms_total / max(s.n, 1):.2f} ms / row)")
    md.append("")
    md.append("## Cost")
    md.append(f"- always-API baseline: **{s.eur_cost_api_only:.4f} €**")
    md.append(f"- Creation OS hybrid:  **{s.eur_cost_hybrid:.4f} €**")
    md.append(f"- savings: **{s.savings_frac:.1%}**  "
              f"({s.eur_cost_api_only - s.eur_cost_hybrid:.4f} € "
              f"saved across {s.n} rows)")
    md.append("")
    md.append("## Accuracy")
    md.append(f"- always-API baseline:   **{s.accuracy_api:.1%}**")
    md.append(f"- BitNet-only baseline:  **{s.accuracy_local:.1%}**")
    md.append(f"- Creation OS hybrid:    **{s.accuracy_hybrid:.1%}**")
    md.append(f"- retained vs API-only: **{s.accuracy_retained:.1%}**")
    md.append("")
    md.append("## Headline")
    md.append(
        f"> Creation OS served **{s.local_share:.0%}** of "
        f"{s.n} queries without touching an API, saved **"
        f"{s.savings_frac:.0%}** of cost, and retained **"
        f"{s.accuracy_retained:.0%}** of always-API accuracy."
    )
    md.append("")
    return "\n".join(md)


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="sigma_pipeline.pipeline_bench",
        description="End-to-end Creation OS pipeline benchmark.",
    )
    ap.add_argument("--input", default=None,
                    help="JSONL fixture; omit to use the built-in demo.")
    ap.add_argument("--output-md", default=None,
                    help="Path for Markdown report; default stdout.")
    ap.add_argument("--output-json", default=None,
                    help="Path for JSON summary; default stderr.")
    ap.add_argument("--log", default=None,
                    help="Per-row JSONL trace path (optional).")
    args = ap.parse_args(argv)

    fixture = _load(pathlib.Path(args.input) if args.input else None)
    rows, summary = run_bench(
        fixture,
        log_path=pathlib.Path(args.log) if args.log else None,
    )
    md = format_markdown(rows, summary)
    if args.output_md:
        pathlib.Path(args.output_md).write_text(md, encoding="utf-8")
    else:
        sys.stdout.write(md)
    j = json.dumps(dataclasses.asdict(summary), indent=2)
    if args.output_json:
        pathlib.Path(args.output_json).write_text(j + "\n",
                                                  encoding="utf-8")
    else:
        sys.stderr.write(j + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
