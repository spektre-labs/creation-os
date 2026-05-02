# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-depth — inference-time adaptive depth using the σ-gate as a halting signal.

This is **not** a recurrent-depth transformer (RDT) or trained ACT head: it is a
**score-only / lab** loop that re-calls ``generate`` or scores a list of candidate
strings, stopping when σ movement falls below a threshold, the gate accepts with
low σ, or σ worsens relative to the previous pass. See ``docs/CLAIM_DISCIPLINE.md``
before conflating this with external “Mythos” scaling laws.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate


class DepthResult:
    """Result of :meth:`AdaptiveDepth.reason` / :meth:`AdaptiveDepth.score_depth`."""

    __slots__ = (
        "text",
        "sigma",
        "verdict",
        "loops_used",
        "max_loops",
        "trace",
        "elapsed_ms",
        "compute_saved",
    )

    def __init__(
        self,
        text: Optional[str],
        sigma: float,
        verdict: str,
        loops_used: int,
        max_loops: int,
        trace: List[Dict[str, Any]],
        elapsed_ms: float,
        compute_saved: str,
    ) -> None:
        self.text = text
        self.sigma = float(sigma)
        self.verdict = str(verdict)
        self.loops_used = int(loops_used)
        self.max_loops = int(max_loops)
        self.trace = trace
        self.elapsed_ms = float(elapsed_ms)
        self.compute_saved = str(compute_saved)

    def __bool__(self) -> bool:
        return self.verdict == "ACCEPT"

    def __repr__(self) -> str:
        return (
            f"DepthResult(σ={self.sigma:.4f}, {self.verdict!r}, "
            f"loops={self.loops_used}/{self.max_loops})"
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "text": self.text,
            "sigma": self.sigma,
            "verdict": self.verdict,
            "loops_used": self.loops_used,
            "max_loops": self.max_loops,
            "trace": self.trace,
            "elapsed_ms": self.elapsed_ms,
            "compute_saved": self.compute_saved,
        }


class AdaptiveDepth:
    """σ-guided halting over repeated generations or scored response lists (ACT-style lab hook)."""

    def __init__(
        self,
        gate: Any = None,
        halt_threshold: float = 0.1,
        max_loops: int = 8,
        min_loops: int = 1,
    ) -> None:
        self.gate = gate or SigmaGate()
        self.halt_threshold = float(halt_threshold)
        self.max_loops = int(max_loops)
        self.min_loops = int(min_loops)

    def reason(
        self,
        prompt: str,
        model: Any = None,
        responses: Optional[List[str]] = None,
        **kwargs: Any,
    ) -> DepthResult:
        t0 = time.monotonic()
        trace: List[Dict[str, Any]] = []
        prev_sigma = 1.0
        best_response: Optional[str] = None
        best_sigma = 1.0

        for loop in range(self.max_loops):
            if responses is not None and loop < len(responses):
                response = responses[loop]
            elif model is not None and hasattr(model, "generate"):
                if best_response:
                    refined_prompt = (
                        f"Previous answer: {best_response}\nImprove and verify: {prompt}"
                    )
                else:
                    refined_prompt = str(prompt)
                response = model.generate(refined_prompt, **kwargs)
            else:
                break

            response = str(response)
            sigma, verdict = self.gate.score(str(prompt), response)
            sigma = float(sigma)
            delta_sigma = prev_sigma - sigma

            trace.append(
                {
                    "loop": loop,
                    "sigma": round(sigma, 4),
                    "delta": round(delta_sigma, 4),
                    "verdict": verdict,
                    "response_len": len(response),
                }
            )

            if sigma < best_sigma:
                best_sigma = sigma
                best_response = response

            if loop >= self.min_loops - 1:
                if abs(delta_sigma) < self.halt_threshold:
                    break
                if verdict == "ACCEPT" and sigma < 0.2:
                    break
                if delta_sigma < 0.0 and loop > 0:
                    break

            prev_sigma = sigma

        elapsed = (time.monotonic() - t0) * 1000.0
        loops_used = len(trace)
        if loops_used == 0:
            return DepthResult(
                text=None,
                sigma=1.0,
                verdict="ABSTAIN",
                loops_used=0,
                max_loops=self.max_loops,
                trace=trace,
                elapsed_ms=round(elapsed, 2),
                compute_saved=f"0/{self.max_loops} loops used",
            )

        return DepthResult(
            text=best_response,
            sigma=best_sigma,
            verdict=self._verdict_from_sigma(best_sigma),
            loops_used=loops_used,
            max_loops=self.max_loops,
            trace=trace,
            elapsed_ms=round(elapsed, 2),
            compute_saved=f"{loops_used}/{self.max_loops} loops used",
        )

    def score_depth(self, prompt: str, responses: List[str]) -> DepthResult:
        """Score-only: measure σ across a depth sweep without a model."""
        return self.reason(prompt, responses=responses)

    @staticmethod
    def _verdict_from_sigma(sigma: float) -> str:
        if sigma < 0.3:
            return "ACCEPT"
        if sigma > 0.8:
            return "ABSTAIN"
        return "RETHINK"


class DepthRouter:
    """Heuristic loop budget from the prompt (depth-aware routing lab helper)."""

    def __init__(self, gate: Any = None) -> None:
        _ = gate  # reserved for σ- or probe-driven allocation

    def allocate(self, prompt: str, max_loops: int = 8) -> int:
        c = self._estimate_complexity(prompt)
        cap = max(1, int(max_loops))
        if c < 0.2:
            return 1
        if c < 0.4:
            return min(2, cap)
        if c < 0.6:
            return min(4, cap)
        if c < 0.8:
            return min(6, cap)
        return cap

    def _estimate_complexity(self, prompt: str) -> float:
        score = 0.0
        n = 0
        prompt_lower = prompt.lower()
        words = prompt.split()

        if len(words) > 50:
            score += 0.5
        elif len(words) > 20:
            score += 0.3
        elif len(words) > 10:
            score += 0.1
        n += 1

        reasoning_words = (
            "prove",
            "derive",
            "explain why",
            "compare and contrast",
            "analyze",
            "evaluate",
            "what would happen if",
            "step by step",
            "reasoning",
            "logic",
            "therefore",
            "contradiction",
            "assumption",
            "theorem",
            "proof",
        )
        reasoning_count = sum(1 for r in reasoning_words if r in prompt_lower)
        score += min(1.0, reasoning_count * 0.25)
        n += 1

        deep_domains = (
            "math",
            "physics",
            "chemistry",
            "proof",
            "theorem",
            "algorithm",
            "complexity",
            "formal",
            "axiom",
            "quantum",
            "topology",
            "category theory",
        )
        domain_count = sum(1 for d in deep_domains if d in prompt_lower)
        score += min(1.0, domain_count * 0.3)
        n += 1

        multi_hop = (
            " and ",
            " then ",
            " after ",
            " because ",
            " since ",
            " if ",
            " given that ",
            " assuming ",
        )
        hop_count = sum(1 for m in multi_hop if m in prompt_lower)
        score += min(1.0, hop_count * 0.15)
        n += 1

        avg = score / max(n, 1)
        if domain_count > 0:
            avg = max(avg, 0.35 + 0.08 * float(domain_count))
        if reasoning_count >= 2:
            avg = max(avg, 0.28 + 0.06 * float(reasoning_count))
        return min(1.0, float(avg))


__all__ = ["AdaptiveDepth", "DepthResult", "DepthRouter"]
