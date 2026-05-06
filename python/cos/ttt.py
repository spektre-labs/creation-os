# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-TTT v2 — test-time adaptation **lab** (scalar ``fast_weights``, no real backward pass).

σ decides whether to apply a cheap update and whether to roll back if stress rises.
No 128K / 2M speed claims; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Sequence, Tuple, Union, cast

__all__ = ["SigmaTTT"]


class SigmaTTT:
    """Trigger-gated nudge to ``fast_weights`` with rollback if σ worsens."""

    def __init__(self, gate: Any = None, *, trigger_threshold: float = 0.45) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.trigger_threshold = float(trigger_threshold)
        self.fast_weights: Dict[str, float] = {"mlp_proj": 1.0}

    def sigma_trigger(self, sigma: float) -> bool:
        return float(sigma) > self.trigger_threshold

    def adapt(
        self,
        a: Any,
        b: Any,
        learning_rate: float = 0.001,
        chunks: int = 5,
    ) -> Union[Dict[str, float], Dict[str, Any]]:
        """Fast-weight nudge **or** threshold nudge on labeled ``(prompt, response, correct)`` pairs.

        * ``adapt(context_tokens: str, fast_weights: dict)`` → updated weights (legacy).
        * ``adapt(examples: list[tuple], gate: SigmaGate, ...)`` → σ report dict (lab TTT).
        """
        del learning_rate  # reserved for smoother future nudges
        if isinstance(a, (list, tuple)) and hasattr(b, "score"):
            if not a:
                z = 0.0
                return {
                    "examples": 0,
                    "σ_before_avg": z,
                    "σ_after_avg": z,
                    "sigma_before_avg": z,
                    "sigma_after_avg": z,
                    "improved": False,
                }
            return self._adapt_labeled_examples(
                cast(List[Tuple[str, str, bool]], list(a)),
                b,
                chunks=int(chunks),
            )
        return self._adapt_fast_weights(str(a), dict(b))

    def _adapt_fast_weights(self, context_tokens: str, fast_weights: Dict[str, float]) -> Dict[str, float]:
        fw = dict(fast_weights)
        h = (hash(context_tokens) % 1001) / 50000.0
        fw["mlp_proj"] = float(fw.get("mlp_proj", 1.0)) * (1.0 - 2.0 * h)
        return fw

    def _adapt_labeled_examples(
        self,
        examples: List[Tuple[str, str, bool]],
        gate: Any,
        *,
        chunks: int,
    ) -> Dict[str, Any]:
        σ_before: List[float] = []
        σ_after: List[float] = []
        n_chunks = max(1, int(chunks))
        step = max(1, (len(examples) + n_chunks - 1) // n_chunks)
        for i in range(0, len(examples), step):
            chunk = examples[i : i + step]
            for prompt, response, correct in chunk:
                σ, _ver = gate.score(str(prompt), str(response))
                σ_before.append(float(σ))
                if float(σ) < float(gate.threshold_accept) and not bool(correct):
                    gate.adjust_threshold("accept", -0.01)
                elif float(σ) > float(gate.threshold_abstain) and bool(correct):
                    gate.adjust_threshold("abstain", 0.01)
                σ_new, _ = gate.score(str(prompt), str(response))
                σ_after.append(float(σ_new))
        s0 = sum(σ_before)
        s1 = sum(σ_after)
        nb = max(len(σ_before), 1)
        na = max(len(σ_after), 1)
        avg_b = s0 / nb
        avg_a = s1 / na
        return {
            "examples": len(examples),
            "σ_before_avg": avg_b,
            "σ_after_avg": avg_a,
            "sigma_before_avg": avg_b,
            "sigma_after_avg": avg_a,
            "improved": s1 < s0,
        }

    def chunk_wise_update(
        self,
        chunks: Sequence[str],
        model: Any,
    ) -> Dict[str, float]:
        del model
        w = dict(self.fast_weights)
        for c in chunks:
            w = self.adapt(str(c), w)
        self.fast_weights = w
        return w

    def ntp_objective_stub(self, context: str, next_char: str) -> float:
        """Toy NTP loss proxy: gate stress on ``context+next_char``."""
        s = float(self.gate.compute_sigma(None, None, str(context)[-64:], str(next_char)))
        return s

    def adapt_with_sigma(
        self,
        sigma_before: float,
        context: str,
        gate: Any,
    ) -> Dict[str, Any]:
        g = gate
        if not self.sigma_trigger(sigma_before):
            return {
                "applied": False,
                "rollback": False,
                "sigma_before": float(sigma_before),
                "sigma_after": float(sigma_before),
                "delta_sigma": 0.0,
            }
        w_prev = dict(self.fast_weights)
        w_new = self.adapt(str(context), w_prev)
        self.fast_weights = w_new
        probe = str(context)[:400]
        sigma_afterprobe = float(g.compute_sigma(None, None, "ttt_probe", probe))
        if sigma_afterprobe > float(sigma_before) + 0.02:
            self.fast_weights = w_prev
            return {
                "applied": False,
                "rollback": True,
                "sigma_before": float(sigma_before),
                "sigma_after": float(sigma_before),
                "delta_sigma": 0.0,
            }
        delta = sigma_afterprobe - float(sigma_before)
        return {
            "applied": True,
            "rollback": False,
            "sigma_before": float(sigma_before),
            "sigma_after": sigma_afterprobe,
            "delta_sigma": round(delta, 6),
        }
