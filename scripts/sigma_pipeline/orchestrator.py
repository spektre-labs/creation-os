# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Full Creation OS pipeline orchestrator.

Chains the six primitives into one call::

    request
      │
      ▼
    [1] Engram lookup       (O(1); HIT with low σ → return immediately)
      │ miss
      ▼
    [2] BitNet generate     (per-token σ-logging via generate_until)
      │
      ▼
    [3] σ-gate              reinforce(σ_mean, τ_accept, τ_rethink):
                              ACCEPT  → return
                              RETHINK → step 4
                              ABSTAIN → return "I do not know"
      │ RETHINK
      ▼
    [4] σ-TTT               update fast weights; regenerate (≤ 3 rounds);
                            if σ drops below τ_accept → return;
                            else fall through.
      │
      ▼
    [5] Escalate            speculative.route(σ_peak, τ_escalate) = ESCALATE
                            → hand off to API model (stubbed unless
                              COS_ORCH_API_KEY is set).
      │
      ▼
    [6] Engram store        key = FNV-1a-64(prompt); stash σ_final so
                            next identical query returns in O(1).

Every step is σ-gated.  Every step is logged.  The whole pipeline is
pure Python over the C primitives' Python mirrors, so a single
``orchestrate(prompt)`` call exercises the entire Creation OS surface
without requiring an LLM to be loaded (thanks to StubBackend).

CLI::

    python -m sigma_pipeline.orchestrator \\
        --backend stub --prompt "What is 2+2?" \\
        --tau-accept 0.30 --tau-rethink 0.70 --tau-escalate 0.75 \\
        --log /tmp/orch_trace.jsonl
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import pathlib
import sys
import time
from typing import Any, Dict, List, Optional

if __package__ in (None, ""):
    _here = pathlib.Path(__file__).resolve().parent.parent
    sys.path.insert(0, str(_here))
    from sigma_pipeline.backends import (                          # noqa: E402
        Backend, BridgeBackend, StubBackend,
    )
    from sigma_pipeline.engram import Engram, fnv1a_64              # noqa: E402
    from sigma_pipeline.generate_until import (                    # noqa: E402
        GenerateUntilEngine, GenerationVerdict,
    )
    from sigma_pipeline.reinforce import (                          # noqa: E402
        Action, RETHINK_SUFFIX, reinforce,
    )
    from sigma_pipeline.speculative import Route, route             # noqa: E402
    from sigma_pipeline import ttt as pttt                          # noqa: E402
else:
    from .backends import Backend, BridgeBackend, StubBackend
    from .engram import Engram, fnv1a_64
    from .generate_until import GenerateUntilEngine, GenerationVerdict
    from .reinforce import Action, RETHINK_SUFFIX, reinforce
    from .speculative import Route, route
    from . import ttt as pttt


# Step constants for the structured log.
STEP_ENGRAM_HIT     = "engram_hit"
STEP_ENGRAM_MISS    = "engram_miss"
STEP_BITNET_ROUND   = "bitnet_round"
STEP_SIGMA_GATE     = "sigma_gate"
STEP_TTT_UPDATE     = "ttt_update"
STEP_ESCALATE       = "escalate"
STEP_ENGRAM_STORE   = "engram_store"


@dataclasses.dataclass
class OrchestratorConfig:
    tau_accept: float = 0.30
    tau_rethink: float = 0.70
    tau_escalate: float = 0.75

    # Engram
    engram_cap: int = 128
    engram_tau_trust: float = 0.30
    engram_long_ttl: int = 1024
    engram_short_ttl: int = 32

    # TTT (a toy fast/slow pair; stands in for the MLP tail).  Even if
    # the underlying backend doesn't expose real weights, we still
    # exercise the primitive so the pipeline path is covered.
    ttt_dim: int = 32
    ttt_lr: float = 0.01
    ttt_tau_sigma: float = 0.50
    ttt_tau_drift: float = 0.30

    # reinforce budget
    max_rethink_rounds: int = 3

    # generate_until
    max_tokens: int = 32

    # API cost model (for cost reporting; no real API call unless key).
    eur_per_local: float = 0.0005
    eur_per_api: float   = 0.02

    # Escalation stub: if no API key, we synthesize a deterministic
    # "API answer" so the pipeline is end-to-end runnable in CI.
    api_key_env: str = "COS_ORCH_API_KEY"


@dataclasses.dataclass
class OrchestratorVerdict:
    answer: str
    origin: str                      # "engram" | "local" | "api"
    rounds: int                      # number of generate_until rounds
    ttt_updates: int                 # TTT steps taken across all rounds
    ttt_resets: int
    engram_hit: bool
    escalated: bool
    abstained: bool
    sigma_final: float
    elapsed_ms: float
    eur_cost: float

    def as_json(self) -> Dict[str, Any]:
        return dataclasses.asdict(self)


class Orchestrator:
    """Stateful across calls: engram + TTT state persist per instance."""

    def __init__(self, backend: Backend, cfg: OrchestratorConfig) -> None:
        self._backend = backend
        self._cfg = cfg
        self._engram = Engram.init(
            cap=cfg.engram_cap,
            tau_trust=cfg.engram_tau_trust,
            long_ttl=cfg.engram_long_ttl,
            short_ttl=cfg.engram_short_ttl,
        )
        slow = [1.0 / (1.0 + 0.01 * i) for i in range(cfg.ttt_dim)]
        self._ttt = pttt.TTTState.init(
            slow=slow, lr=cfg.ttt_lr,
            tau_sigma=cfg.ttt_tau_sigma, tau_drift=cfg.ttt_tau_drift,
        )

    # ----------------------------------------------------------------- #

    def orchestrate(self, prompt: str,
                    log_path: Optional[pathlib.Path] = None
                    ) -> OrchestratorVerdict:
        events: List[Dict[str, Any]] = []
        t0 = time.time()

        # ------------ [1] engram lookup ------------
        key = fnv1a_64(prompt)
        hit = self._engram.get(key)
        if hit is not None and hit.sigma_at_store <= self._cfg.tau_accept:
            events.append({
                "step": STEP_ENGRAM_HIT, "key": key,
                "sigma_at_store": hit.sigma_at_store,
                "access_count": hit.access_count,
            })
            elapsed = (time.time() - t0) * 1000.0
            verdict = OrchestratorVerdict(
                answer=str(hit.value),
                origin="engram",
                rounds=0,
                ttt_updates=0,
                ttt_resets=0,
                engram_hit=True,
                escalated=False,
                abstained=False,
                sigma_final=hit.sigma_at_store,
                elapsed_ms=elapsed,
                eur_cost=0.0,  # cache hit is free
            )
            self._finalize_log(log_path, prompt, events, verdict)
            return verdict
        events.append({"step": STEP_ENGRAM_MISS, "key": key})

        # ------------ [2..4] local generate + σ-gate + TTT loop ------------
        engine = GenerateUntilEngine(
            backend=self._backend,
            tau_accept=self._cfg.tau_accept,
            tau_rethink=self._cfg.tau_rethink,
            tau_escalate=self._cfg.tau_escalate,
            max_tokens=self._cfg.max_tokens,
        )
        current_prompt = prompt
        last_verdict: Optional[GenerationVerdict] = None
        ttt_updates_before = self._ttt.n_steps_updated
        ttt_resets_before = self._ttt.n_resets

        for rnd in range(self._cfg.max_rethink_rounds):
            last_verdict = engine.run(current_prompt)
            events.append({
                "step": STEP_BITNET_ROUND,
                "round": rnd,
                "tokens": last_verdict.tokens,
                "sigma_peak": last_verdict.sigma_peak,
                "sigma_mean_avg": last_verdict.sigma_mean_avg,
                "terminal_action": last_verdict.terminal_action.name,
                "terminal_route": last_verdict.terminal_route.name,
            })

            action = reinforce(
                last_verdict.sigma_mean_avg,
                self._cfg.tau_accept, self._cfg.tau_rethink,
            )
            events.append({
                "step": STEP_SIGMA_GATE, "round": rnd,
                "sigma_in": last_verdict.sigma_mean_avg,
                "action": action.name,
            })
            if action == Action.ACCEPT:
                break

            # Escalation wins over RETHINK if σ_peak hit τ_escalate at
            # a segment boundary.  generate_until already reports that
            # via terminal_route.
            if last_verdict.terminal_route == Route.ESCALATE:
                break

            if action == Action.ABSTAIN:
                break

            # RETHINK round: take one TTT step (σ-gated) to "adapt" the
            # model to this context, then re-prompt with the CoT suffix.
            self._ttt_step(last_verdict.sigma_mean_avg)
            events.append({
                "step": STEP_TTT_UPDATE, "round": rnd,
                "sigma_in": last_verdict.sigma_mean_avg,
                "n_steps_updated": self._ttt.n_steps_updated,
                "n_resets": self._ttt.n_resets,
            })
            current_prompt = current_prompt + RETHINK_SUFFIX

        assert last_verdict is not None  # loop ran ≥ 1 time

        # ------------ [5] escalate if needed ------------
        origin = "local"
        answer = last_verdict.text
        escalated = False
        abstained = False
        sigma_final = last_verdict.sigma_mean_avg
        eur_cost = self._cfg.eur_per_local

        terminal_route = route(last_verdict.sigma_peak,
                               self._cfg.tau_escalate)
        terminal_action = reinforce(last_verdict.sigma_mean_avg,
                                    self._cfg.tau_accept,
                                    self._cfg.tau_rethink)

        if terminal_action == Action.ABSTAIN:
            answer = "I do not know."
            abstained = True
            sigma_final = last_verdict.sigma_mean_avg
        elif terminal_route == Route.ESCALATE or \
             terminal_action == Action.RETHINK:
            # After max rethink rounds still uncertain → escalate.
            answer, eur_cost = self._escalate(prompt, last_verdict)
            origin = "api"
            escalated = True
            sigma_final = 0.10  # API assumed confident by policy
            events.append({
                "step": STEP_ESCALATE,
                "sigma_peak": last_verdict.sigma_peak,
                "sigma_mean_avg": last_verdict.sigma_mean_avg,
                "eur_cost": eur_cost,
            })

        # ------------ [6] engram store ------------
        _rc, evicted = self._engram.put(key, sigma_final, answer)
        events.append({
            "step": STEP_ENGRAM_STORE,
            "key": key, "sigma": sigma_final,
            "evicted_key": evicted,
        })

        elapsed = (time.time() - t0) * 1000.0
        verdict = OrchestratorVerdict(
            answer=answer,
            origin=origin,
            rounds=sum(1 for e in events if e["step"] == STEP_BITNET_ROUND),
            ttt_updates=self._ttt.n_steps_updated - ttt_updates_before,
            ttt_resets=self._ttt.n_resets - ttt_resets_before,
            engram_hit=False,
            escalated=escalated,
            abstained=abstained,
            sigma_final=sigma_final,
            elapsed_ms=elapsed,
            eur_cost=eur_cost,
        )
        self._finalize_log(log_path, prompt, events, verdict)
        return verdict

    # ----------------------------------------------------------------- #

    def _ttt_step(self, sigma_in: float) -> None:
        # Synthetic gradient: small, sign-varied; the primitive already
        # handles NaN / inf component-wise, so any real backend gradient
        # (when wired) would slot in here unchanged.
        g = [0.01 * (1.0 if (i & 1) == 0 else -1.0)
             for i in range(self._cfg.ttt_dim)]
        pttt.step(self._ttt, sigma_in, g)
        pttt.reset_if_drift(self._ttt)

    def _escalate(self, prompt: str,
                  verdict: GenerationVerdict) -> tuple[str, float]:
        """Hand off to an API model.  If no key is configured we emit a
        deterministic synthetic answer so CI can exercise the
        ESCALATE path without network. """
        key = os.environ.get(self._cfg.api_key_env, "")
        if not key:
            return ("[api-stub] escalated: σ_peak="
                    f"{verdict.sigma_peak:.3f} "
                    f"σ_mean={verdict.sigma_mean_avg:.3f} "
                    f"prefix={verdict.text[:40]!r}",
                    self._cfg.eur_per_api)
        # Real API call would go here; intentionally not implemented in
        # this kernel commit — the pipeline is the product, the API
        # adapter is a later integration.
        return (f"[api-live] (implementation deferred; key present)",
                self._cfg.eur_per_api)

    def _finalize_log(self, path: Optional[pathlib.Path],
                      prompt: str, events: List[Dict[str, Any]],
                      verdict: OrchestratorVerdict) -> None:
        if path is None:
            return
        path = pathlib.Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("w", encoding="utf-8") as fh:
            fh.write(json.dumps({
                "kind": "start",
                "prompt": prompt,
                "cfg": dataclasses.asdict(self._cfg),
            }) + "\n")
            for e in events:
                fh.write(json.dumps({"kind": "event", **e}) + "\n")
            fh.write(json.dumps({
                "kind": "summary",
                **verdict.as_json(),
            }) + "\n")


# -- CLI --------------------------------------------------------------- #


def _make_backend(name: str, extra: Dict[str, Any]) -> Backend:
    if name == "stub":
        # StubBackend is deterministic via its hard-coded token /
        # sigma-trace pair; max_steps just bounds the generator.
        return StubBackend(max_steps=int(extra.get("max_tokens", 64)) * 4)
    if name == "bridge":
        bridge = extra.get("bridge", "./creation_os_v101")
        gguf   = extra.get("gguf", "")
        n_ctx  = int(extra.get("n_ctx", 4096))
        return BridgeBackend(bridge=bridge, gguf=gguf, n_ctx=n_ctx)
    raise ValueError(f"unknown backend: {name}")


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        prog="sigma_pipeline.orchestrator",
        description="Run the full Creation OS pipeline once.",
    )
    ap.add_argument("--backend", default="stub", choices=["stub", "bridge"])
    ap.add_argument("--prompt", required=True)
    ap.add_argument("--tau-accept",   type=float, default=0.30)
    ap.add_argument("--tau-rethink",  type=float, default=0.70)
    ap.add_argument("--tau-escalate", type=float, default=0.75)
    ap.add_argument("--max-tokens",   type=int,   default=32)
    ap.add_argument("--max-rethink",  type=int,   default=3)
    ap.add_argument("--log", default=None)
    ap.add_argument("--bridge", default="./creation_os_v101")
    ap.add_argument("--gguf", default="")
    ap.add_argument("--n-ctx", type=int, default=4096)
    args = ap.parse_args(argv)

    cfg = OrchestratorConfig(
        tau_accept=args.tau_accept,
        tau_rethink=args.tau_rethink,
        tau_escalate=args.tau_escalate,
        max_tokens=args.max_tokens,
        max_rethink_rounds=args.max_rethink,
    )
    backend = _make_backend(args.backend, {
        "max_tokens": max(args.max_tokens, 1),
        "bridge": args.bridge,
        "gguf": args.gguf,
        "n_ctx": args.n_ctx,
    })
    orch = Orchestrator(backend=backend, cfg=cfg)
    log = pathlib.Path(args.log) if args.log else None
    v = orch.orchestrate(args.prompt, log_path=log)
    # One-line JSON summary on stdout for scripts; full verdict on stderr.
    print(json.dumps(v.as_json()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
