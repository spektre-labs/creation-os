# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""v103.1 — generate_until with per-token σ-logging.

The blocker that prevented free-form generation from being σ-gated.  This
module drives a ``Backend`` one token at a time, records a full per-step
σ trace as JSONL, and applies the policy primitives (reinforce /
speculative) *during* generation:

  1. For each new token, the backend returns σ_mean / σ_max / σ_product
     + an 8-channel σ_profile.
  2. We keep a running σ_peak (peak_update), and at each segment boundary
     (sentence / paragraph) we ask speculative.route(σ_peak, τ_escalate):
        LOCAL    → keep generating
        ESCALATE → stop, mark the prefix for API hand-off
  3. At every step we also consult reinforce(σ_mean, τ_accept, τ_rethink):
        ACCEPT   → continue
        RETHINK  → stop, caller may re-prompt with a first-principles suffix
        ABSTAIN  → stop, mark the answer as "I do not know"

The driver is pure Python.  No PyTorch required.  Works on any host with
either the compiled v101 bridge (real BitNet) or the StubBackend
(deterministic fake, CI default).

CLI usage::

    python -m sigma_pipeline.generate_until \\
        --backend stub --prompt "What is 2+2?" \\
        --tau-accept 0.30 --tau-rethink 0.70 --tau-escalate 0.75 \\
        --max-tokens 32 \\
        --log /tmp/sigma_trace.jsonl

The JSONL log contains one record per token plus a final ``summary``
record with verdict / total tokens / peak σ / elapsed ms.
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import pathlib
import sys
import time
from typing import IO, Any, Dict, List, Optional

# Allow running as ``python -m sigma_pipeline.generate_until`` and as
# ``python scripts/sigma_pipeline/generate_until.py`` alike.
if __package__ in (None, ""):
    _here = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(_here))
    from sigma_pipeline.backends import (                    # noqa: E402
        Backend, BridgeBackend, StubBackend, TokenStep,
    )
    from sigma_pipeline.reinforce import Action, reinforce    # noqa: E402
    from sigma_pipeline.speculative import (                  # noqa: E402
        Route, is_segment_boundary, peak_update, route,
    )
else:
    from .backends import (
        Backend, BridgeBackend, StubBackend, TokenStep,
    )
    from .reinforce import Action, reinforce
    from .speculative import Route, is_segment_boundary, peak_update, route


@dataclasses.dataclass
class GenerationVerdict:
    text: str
    tokens: int
    elapsed_ms: float
    sigma_peak: float
    sigma_mean_avg: float
    terminal_action: Action
    terminal_route: Route
    segments: int
    eos: bool

    def as_json(self) -> Dict[str, Any]:
        d = dataclasses.asdict(self)
        d["terminal_action"] = self.terminal_action.name
        d["terminal_route"] = self.terminal_route.name
        return d


class GenerateUntilEngine:
    """Per-token σ logger + live policy evaluator.

    Not thread-safe.  One engine per (prompt, backend) pair; call
    :py:meth:`run` to generate and stream σ to the log.
    """

    def __init__(
        self,
        backend: Backend,
        tau_accept: float = 0.30,
        tau_rethink: float = 0.70,
        tau_escalate: float = 0.75,
        max_tokens: int = 128,
        log_fh: Optional[IO[str]] = None,
    ) -> None:
        if not (tau_accept <= tau_rethink <= tau_escalate):
            raise ValueError(
                f"thresholds must satisfy "
                f"tau_accept ({tau_accept}) <= "
                f"tau_rethink ({tau_rethink}) <= "
                f"tau_escalate ({tau_escalate})"
            )
        self._backend = backend
        self._tau_accept = float(tau_accept)
        self._tau_rethink = float(tau_rethink)
        self._tau_escalate = float(tau_escalate)
        self._max_tokens = int(max_tokens)
        self._log_fh = log_fh

    def _emit(self, rec: Dict[str, Any]) -> None:
        if self._log_fh is not None:
            self._log_fh.write(json.dumps(rec, sort_keys=True) + "\n")
            self._log_fh.flush()

    def run(self, prompt: str) -> GenerationVerdict:
        t0 = time.monotonic()
        accumulated = ""
        peak = 0.0
        segments = 0
        sum_mean = 0.0
        terminal_action = Action.ACCEPT
        terminal_route = Route.LOCAL
        eos = False

        self._emit({
            "kind": "start",
            "prompt": prompt,
            "tau_accept": self._tau_accept,
            "tau_rethink": self._tau_rethink,
            "tau_escalate": self._tau_escalate,
            "max_tokens": self._max_tokens,
            "t_ms": 0.0,
        })

        step = 0
        while step < self._max_tokens:
            ts: TokenStep = self._backend.step(prompt, accumulated)
            if ts.eos and ts.token_text == "":
                eos = True
                break

            accumulated += ts.token_text
            peak = peak_update(peak, ts.sigma_max)
            sum_mean += ts.sigma_mean

            action = reinforce(
                ts.sigma_mean, self._tau_accept, self._tau_rethink
            )

            last_char = ts.token_text[-1:] if ts.token_text else ""
            at_boundary = is_segment_boundary(last_char)
            rt = route(peak, self._tau_escalate)

            elapsed = (time.monotonic() - t0) * 1000.0
            self._emit({
                "kind": "token",
                "step": ts.step,
                "token": ts.token_text,
                "sigma_mean": ts.sigma_mean,
                "sigma_max": ts.sigma_max,
                "sigma_product": ts.sigma_product,
                "sigma_profile": ts.sigma_profile,
                "sigma_peak": peak,
                "action": action.name,
                "route": rt.name,
                "segment_boundary": at_boundary,
                "t_ms": elapsed,
            })

            if at_boundary:
                segments += 1

            terminal_action = action
            terminal_route = rt

            if action is Action.RETHINK:
                break
            if action is Action.ABSTAIN:
                break
            if rt is Route.ESCALATE and at_boundary:
                # Hand off at a segment boundary so the big model never
                # resumes mid-word.
                break

            if ts.eos:
                eos = True
                break
            step += 1

        elapsed_ms = (time.monotonic() - t0) * 1000.0
        tokens = step + (0 if eos else 1)
        mean_avg = (sum_mean / tokens) if tokens > 0 else 0.0
        verdict = GenerationVerdict(
            text=accumulated,
            tokens=tokens,
            elapsed_ms=elapsed_ms,
            sigma_peak=peak,
            sigma_mean_avg=mean_avg,
            terminal_action=terminal_action,
            terminal_route=terminal_route,
            segments=segments,
            eos=eos,
        )
        self._emit({"kind": "summary", **verdict.as_json()})
        return verdict


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def _build_backend(args: argparse.Namespace) -> Backend:
    if args.backend == "stub":
        return StubBackend(max_steps=args.max_tokens * 4)
    if args.backend == "bridge":
        if not args.bridge:
            raise SystemExit("--bridge <path> required for --backend bridge")
        if not args.gguf:
            raise SystemExit("--gguf <path> required for --backend bridge")
        return BridgeBackend(args.bridge, args.gguf, n_ctx=args.n_ctx)
    raise SystemExit(f"unknown backend: {args.backend}")


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="Generate until σ-gate: per-token σ logging + live policy"
    )
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--backend", default="stub", choices=("stub", "bridge"))
    ap.add_argument("--bridge", default=None,
                    help="path to compiled ./creation_os_v101 (bridge only)")
    ap.add_argument("--gguf", default=None,
                    help="path to BitNet gguf (bridge only)")
    ap.add_argument("--n-ctx", type=int, default=4096)
    ap.add_argument("--tau-accept",   type=float, default=0.30)
    ap.add_argument("--tau-rethink",  type=float, default=0.70)
    ap.add_argument("--tau-escalate", type=float, default=0.75)
    ap.add_argument("--max-tokens", type=int, default=64)
    ap.add_argument("--log", default=None,
                    help="path to JSONL σ-trace output (default: stderr)")
    args = ap.parse_args(argv)

    backend = _build_backend(args)
    log_fh = open(args.log, "w", encoding="utf-8") if args.log else sys.stderr
    try:
        engine = GenerateUntilEngine(
            backend,
            tau_accept=args.tau_accept,
            tau_rethink=args.tau_rethink,
            tau_escalate=args.tau_escalate,
            max_tokens=args.max_tokens,
            log_fh=log_fh,
        )
        verdict = engine.run(args.prompt)
    finally:
        backend.close()
        if log_fh is not sys.stderr:
            log_fh.close()

    out = {
        "text": verdict.text,
        "tokens": verdict.tokens,
        "elapsed_ms": round(verdict.elapsed_ms, 3),
        "sigma_peak": verdict.sigma_peak,
        "sigma_mean_avg": verdict.sigma_mean_avg,
        "terminal_action": verdict.terminal_action.name,
        "terminal_route": verdict.terminal_route.name,
        "segments": verdict.segments,
        "eos": verdict.eos,
    }
    print(json.dumps(out, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
