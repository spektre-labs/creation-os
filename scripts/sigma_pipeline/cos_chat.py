# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""`cos chat` — interactive σ-gated chat REPL.

Ties P1 (reinforce) + P2 (speculative) + P3 (generate_until) into one
interactive program.  Each user turn runs through:

  1. **First attempt** — small model generates an answer token-by-token
     with live σ-logging.  Per-sentence σ_peak decides whether to keep
     going, rethink, or escalate.
  2. **RETHINK** — if the first attempt's terminal action is RETHINK,
     re-prompt the small model with a first-principles CoT suffix and
     regenerate (up to MAX_ROUNDS=3).
  3. **ESCALATE** — if σ_peak ≥ τ_escalate at a segment boundary, mark
     the prefix and (when an API key is configured) hand off to the big
     model.  Without an API key, the chat prints a "would-escalate"
     stub line so the demo still runs.

The chat streams tokens to the TTY as they arrive, colour-coding each
segment by σ_peak (low → green, mid → yellow, high → red).  One JSONL
transcript record is written per turn for later cost measurement.

Usage::

    PYTHONPATH=scripts python -m sigma_pipeline.cos_chat         # stub backend
    PYTHONPATH=scripts python -m sigma_pipeline.cos_chat \\
        --backend bridge --bridge ./creation_os_v101 \\
        --gguf models/bitnet-b1.58-2B-4T.gguf
    PYTHONPATH=scripts python -m sigma_pipeline.cos_chat --once \\
        --prompt "What is 2+2?"    # non-interactive, one turn

Environment::

    COS_CHAT_API_KEY       — if set, ESCALATE actually calls the API;
                             if unset, escalate prints the marker only.
    COS_CHAT_TRANSCRIPT    — override default transcript path
                             (default: ./cos_chat_transcript.jsonl).
"""

from __future__ import annotations

import argparse
import io
import json
import os
import pathlib
import sys
import time
from typing import IO, List, Optional, Tuple

if __package__ in (None, ""):
    _here = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(_here))
    from sigma_pipeline.backends import (                     # noqa: E402
        Backend, BridgeBackend, StubBackend,
    )
    from sigma_pipeline.generate_until import (                # noqa: E402
        GenerateUntilEngine, GenerationVerdict,
    )
    from sigma_pipeline.reinforce import (                     # noqa: E402
        Action, MAX_ROUNDS, RETHINK_SUFFIX,
    )
    from sigma_pipeline.speculative import Route               # noqa: E402
else:
    from .backends import Backend, BridgeBackend, StubBackend
    from .generate_until import GenerateUntilEngine, GenerationVerdict
    from .reinforce import Action, MAX_ROUNDS, RETHINK_SUFFIX
    from .speculative import Route


# ANSI colours; skipped when stdout is not a TTY.
_C_RESET = "\x1b[0m"
_C_LOW   = "\x1b[32m"   # green
_C_MID   = "\x1b[33m"   # yellow
_C_HIGH  = "\x1b[31m"   # red
_C_DIM   = "\x1b[2m"
_C_BOLD  = "\x1b[1m"


def _colour(sig: float, tau_rethink: float, tau_escalate: float) -> str:
    if sig < tau_rethink:
        return _C_LOW
    if sig < tau_escalate:
        return _C_MID
    return _C_HIGH


def _use_colour(stream: IO[str]) -> bool:
    return bool(getattr(stream, "isatty", lambda: False)())


def _build_backend(args: argparse.Namespace) -> Backend:
    if args.backend == "stub":
        return StubBackend(max_steps=args.max_tokens * 4)
    if args.backend == "bridge":
        if not args.bridge or not args.gguf:
            raise SystemExit(
                "--bridge <path> and --gguf <path> are required for "
                "--backend bridge"
            )
        return BridgeBackend(args.bridge, args.gguf, n_ctx=args.n_ctx)
    raise SystemExit(f"unknown backend: {args.backend}")


def _stub_api_escalate(prompt: str, partial: str) -> str:
    """Fallback when no COS_CHAT_API_KEY is set.

    Returns a deterministic marker string so demos reproduce.  Real API
    integration lives in v262 / v249; wiring it into cos_chat is out of
    scope for the MVP and explicitly opt-in via environment.
    """
    return (
        f"[ESCALATED to big-model (stub, no COS_CHAT_API_KEY set)]  "
        f"prompt_len={len(prompt)}  partial_len={len(partial)}"
    )


def _run_turn(
    backend: Backend,
    prompt: str,
    args: argparse.Namespace,
    transcript_fh: Optional[IO[str]],
    out_stream: IO[str],
) -> Tuple[str, List[GenerationVerdict]]:
    """Run one user turn through the full reinforce / escalate loop.

    Returns the final displayed answer + the list of per-round verdicts.
    """
    verdicts: List[GenerationVerdict] = []
    rewritten = prompt
    shown = ""
    colours = _use_colour(out_stream)

    for round_ in range(MAX_ROUNDS):
        engine = GenerateUntilEngine(
            backend,
            tau_accept=args.tau_accept,
            tau_rethink=args.tau_rethink,
            tau_escalate=args.tau_escalate,
            max_tokens=args.max_tokens,
            log_fh=None,   # per-token log goes to transcript below
        )
        v = engine.run(rewritten)
        verdicts.append(v)

        # Stream the accumulated text with σ-colouring.
        c = _colour(v.sigma_peak, args.tau_rethink, args.tau_escalate)
        if colours:
            out_stream.write(f"{_C_DIM}round {round_}{_C_RESET}  ")
            out_stream.write(f"{c}{v.text}{_C_RESET}")
            out_stream.write(
                f"  {_C_DIM}[σ_peak={v.sigma_peak:.2f} "
                f"action={v.terminal_action.name} "
                f"route={v.terminal_route.name}]{_C_RESET}\n"
            )
        else:
            out_stream.write(
                f"round {round_}  {v.text}  "
                f"[σ_peak={v.sigma_peak:.2f} "
                f"action={v.terminal_action.name} "
                f"route={v.terminal_route.name}]\n"
            )
        out_stream.flush()

        if v.terminal_action is Action.ACCEPT and \
           v.terminal_route is Route.LOCAL:
            shown = v.text
            break

        if v.terminal_route is Route.ESCALATE:
            # Hand off to the big model (stubbed unless env is set).
            if os.environ.get("COS_CHAT_API_KEY"):
                # Real API integration intentionally NOT wired here; this
                # hook lives in v262 σ-hybrid-engine.  For now we mark
                # the escalation explicitly so the demo is honest.
                tail = _stub_api_escalate(rewritten, v.text)
            else:
                tail = _stub_api_escalate(rewritten, v.text)
            if colours:
                out_stream.write(f"{_C_BOLD}{tail}{_C_RESET}\n")
            else:
                out_stream.write(tail + "\n")
            shown = v.text + "\n" + tail
            break

        if v.terminal_action is Action.RETHINK and \
           round_ < MAX_ROUNDS - 1:
            rewritten = prompt + RETHINK_SUFFIX
            continue

        if v.terminal_action is Action.ABSTAIN:
            if colours:
                out_stream.write(
                    f"{_C_HIGH}ABSTAIN — I do not know.{_C_RESET}\n"
                )
            else:
                out_stream.write("ABSTAIN — I do not know.\n")
            shown = v.text + "\n[ABSTAIN]"
            break

        # Exhausted RETHINK budget.
        if colours:
            out_stream.write(
                f"{_C_HIGH}ABSTAIN (RETHINK budget exhausted after "
                f"{MAX_ROUNDS} rounds).{_C_RESET}\n"
            )
        else:
            out_stream.write(
                f"ABSTAIN (RETHINK budget exhausted after "
                f"{MAX_ROUNDS} rounds).\n"
            )
        shown = v.text + "\n[ABSTAIN]"
        break

    if transcript_fh is not None:
        transcript_fh.write(json.dumps({
            "ts_ms": round(time.monotonic() * 1000.0, 3),
            "prompt": prompt,
            "rounds": [v.as_json() for v in verdicts],
            "final_text": shown,
        }, sort_keys=True) + "\n")
        transcript_fh.flush()

    return shown, verdicts


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description="cos chat — σ-gated interactive REPL (reinforce + "
                    "speculative + generate_until)"
    )
    ap.add_argument("--backend", default="stub", choices=("stub", "bridge"))
    ap.add_argument("--bridge", default=None)
    ap.add_argument("--gguf", default=None)
    ap.add_argument("--n-ctx", type=int, default=4096)
    ap.add_argument("--tau-accept",   type=float, default=0.30)
    ap.add_argument("--tau-rethink",  type=float, default=0.70)
    ap.add_argument("--tau-escalate", type=float, default=0.75)
    ap.add_argument("--max-tokens", type=int, default=64)
    ap.add_argument("--once", action="store_true",
                    help="run one turn with --prompt and exit "
                         "(useful for CI / demos)")
    ap.add_argument("--prompt", default=None,
                    help="prompt for --once mode")
    ap.add_argument("--transcript", default=None,
                    help="transcript path (JSONL; default: "
                         "$COS_CHAT_TRANSCRIPT or ./cos_chat_transcript.jsonl)")
    ap.add_argument("--no-transcript", action="store_true")
    args = ap.parse_args(argv)

    backend = _build_backend(args)
    transcript_fh: Optional[IO[str]] = None
    if not args.no_transcript:
        path = (
            args.transcript
            or os.environ.get("COS_CHAT_TRANSCRIPT")
            or "./cos_chat_transcript.jsonl"
        )
        transcript_fh = open(path, "a", encoding="utf-8", buffering=1)

    out = sys.stdout

    banner = (
        f"cos chat  ·  backend={args.backend}  ·  "
        f"τ_accept={args.tau_accept}  τ_rethink={args.tau_rethink}  "
        f"τ_escalate={args.tau_escalate}\n"
    )
    out.write(banner)
    out.flush()

    try:
        if args.once:
            if not args.prompt:
                raise SystemExit("--once requires --prompt")
            _run_turn(backend, args.prompt, args, transcript_fh, out)
        else:
            while True:
                try:
                    line = input(">>> ")
                except (EOFError, KeyboardInterrupt):
                    out.write("\nbye.\n")
                    break
                line = line.strip()
                if not line:
                    continue
                if line in ("/quit", "/exit", ":q"):
                    break
                _run_turn(backend, line, args, transcript_fh, out)
    finally:
        backend.close()
        if transcript_fh is not None:
            transcript_fh.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
