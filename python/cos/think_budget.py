# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-adaptive test-time compute (lab): depth search, branching policy, marginal stopping.

σ summarizes prompt–response strain for a **candidate** reasoning trace. This module uses it as a
**cheap proxy** for “keep extending thought?” vs “stop / branch more?” — analogous in spirit to
entropy-aware branching papers, but **not** a reproduction of EAGER / CODA and **not** a claim of
``65%`` or ``118×`` without an archived harness (see ``docs/CLAIM_DISCIPLINE.md``).

**NOT AGI ACHIEVED** — lab scaffolding only."""
from __future__ import annotations

from typing import Any, Callable, Dict, List

from cos.sigma_gate import SigmaGate

__all__ = ["ThinkBudget"]

ThinkFn = Callable[..., str]


def _verdict_str(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class ThinkBudget:
    """Bounded depth search and branch sizing from σ traces."""

    def __init__(self, gate: Any = None, max_tokens: int = 2048) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.max_tokens = max(1, int(max_tokens))

    def optimal_depth(
        self,
        prompt: str,
        think_fn: ThinkFn,
        max_steps: int = 8,
    ) -> Dict[str, Any]:
        """Extend ``think_fn(prompt, depth=…)`` until σ rises for three consecutive steps."""
        σ_trace: List[float] = []
        responses: List[str] = []
        best_step = 0
        best_σ = 1.0
        max_steps = max(1, int(max_steps))
        per_step_budget = max(1, self.max_tokens // max_steps)

        for step in range(max_steps):
            response = think_fn(str(prompt), depth=step)
            σ, verdict = self.gate.score(str(prompt), str(response))
            σ = float(σ)
            _ = _verdict_str(verdict)

            σ_trace.append(σ)
            responses.append(response)

            if σ < best_σ:
                best_σ = σ
                best_step = step

            if len(σ_trace) >= 3:
                a, b, c = σ_trace[-3], σ_trace[-2], σ_trace[-1]
                if c > b > a:
                    break

        steps_run = len(σ_trace)
        tokens_saved = max(0, (max_steps - steps_run) * per_step_budget)
        return {
            "best_step": best_step,
            "best_σ": round(float(best_σ), 4),
            "best_response": responses[best_step],
            "total_steps": steps_run,
            "σ_trace": [round(float(s), 4) for s in σ_trace],
            "overthinking_detected": bool(steps_run > best_step + 2),
            "tokens_saved": int(tokens_saved),
        }

    def branch_or_not(self, prompt: str, response: str) -> Dict[str, Any]:
        """EAGER-style heuristic: more candidate paths only when σ is high."""
        σ, verdict = self.gate.score(str(prompt), str(response))
        σ = float(σ)
        v = _verdict_str(verdict)
        if σ < 0.15:
            return {
                "branch": False,
                "reason": "confident — one path enough",
                "σ": round(σ, 4),
                "verdict": v,
            }
        if σ < 0.4:
            return {
                "branch": False,
                "reason": "moderate — stay on path",
                "σ": round(σ, 4),
                "n_paths": 1,
                "verdict": v,
            }
        if σ < 0.7:
            return {
                "branch": True,
                "reason": "uncertain — try 3 paths",
                "σ": round(σ, 4),
                "n_paths": 3,
                "verdict": v,
            }
        return {
            "branch": True,
            "reason": "very uncertain — try 5 paths",
            "σ": round(σ, 4),
            "n_paths": 5,
            "verdict": v,
        }

    def marginal_gain(self, σ_trace: List[float]) -> Dict[str, Any]:
        """Δσ between successive steps (positive ⇒ σ fell ⇒ coherence improved)."""
        if len(σ_trace) < 2:
            return {"keep_thinking": True, "marginal": 0.5}

        gains = [float(σ_trace[i]) - float(σ_trace[i + 1]) for i in range(len(σ_trace) - 1)]
        recent_gain = float(gains[-1])
        total_improvement = float(σ_trace[0]) - float(σ_trace[-1])

        if recent_gain > 0.01:
            rec = "keep thinking — still improving"
        elif recent_gain > -0.01:
            rec = "stop — marginal gain negligible"
        else:
            rec = "STOP — overthinking, σ rising"

        return {
            "keep_thinking": bool(recent_gain > 0.01),
            "marginal_gain": round(recent_gain, 4),
            "total_improvement": round(total_improvement, 4),
            "diminishing_returns": bool(recent_gain < 0.01 and len(gains) > 2),
            "recommendation": rec,
        }

    def compute_budget(self, prompt: str) -> Dict[str, Any]:
        """Map a coarse difficulty σ to token / path / depth knobs (order-of-magnitude lab table)."""
        σ_est, _verdict = self.gate.score("difficulty estimate", str(prompt))
        σ_est = float(σ_est)

        if σ_est < 0.2:
            budget = {"tokens": 256, "paths": 1, "depth": 2}
        elif σ_est < 0.5:
            budget = {"tokens": 1024, "paths": 1, "depth": 4}
        elif σ_est < 0.7:
            budget = {"tokens": 2048, "paths": 3, "depth": 6}
        else:
            budget = {"tokens": 4096, "paths": 5, "depth": 8}

        budget["σ_estimate"] = round(σ_est, 4)
        return budget
