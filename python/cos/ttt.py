# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-TTT v2 — test-time adaptation **lab** (scalar ``fast_weights``, no real backward pass).

σ decides whether to apply a cheap update and whether to roll back if stress rises.
No 128K / 2M speed claims; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Sequence, Tuple

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
        arg0: Any,
        arg1: Any | None = None,
        *,
        learning_rate: float = 0.001,
        chunks: int = 5,
        gate: Any | None = None,
    ) -> Any:
        """Fast-weight nudge (str, dict) **or** σ-gate threshold TTT (examples list, gate)."""
        if isinstance(arg0, str) and isinstance(arg1, dict):
            return self._adapt_fast_weights(arg0, arg1)
        examples = arg0
        g = arg1 if gate is None else gate
        return self._adapt_thresholds_from_examples(examples, g, learning_rate=learning_rate, chunks=chunks)

    def _adapt_fast_weights(self, context_tokens: str, fast_weights: Dict[str, float]) -> Dict[str, float]:
        fw = dict(fast_weights)
        h = (hash(context_tokens) % 1001) / 50000.0
        fw["mlp_proj"] = float(fw.get("mlp_proj", 1.0)) * (1.0 - 2.0 * h)
        return fw

    def _adapt_thresholds_from_examples(
        self,
        examples: Sequence[Tuple[str, str, bool]],
        gate: Any,
        *,
        learning_rate: float = 0.001,
        chunks: int = 5,
    ) -> Dict[str, Any]:
        """Chunk-wise threshold nudge from labeled (prompt, response, correct) rows (lab)."""
        if not examples:
            return {
                "examples": 0,
                "σ_before_avg": 0.0,
                "σ_after_avg": 0.0,
                "improved": False,
            }
        rows: List[Tuple[str, str, bool]] = [(str(a[0]), str(a[1]), bool(a[2])) for a in examples]
        n_chunk = max(1, int(chunks))
        size = max(1, (len(rows) + n_chunk - 1) // n_chunk)
        σ_before: List[float] = []
        σ_after: List[float] = []
        scale = float(learning_rate) / 0.001
        for i in range(0, len(rows), size):
            chunk = rows[i : i + size]
            for prompt, response, correct in chunk:
                σ, _ = gate.score(prompt, response)
                σ_before.append(float(σ))
                t_accept = float(getattr(gate, "threshold_accept", 0.15))
                t_abstain = float(getattr(gate, "threshold_abstain", 0.85))
                if float(σ) < t_accept and not correct:
                    gate.adjust_threshold("accept", -0.01 * scale)
                elif float(σ) > t_abstain and correct:
                    gate.adjust_threshold("abstain", 0.01 * scale)
                σ_new, _ = gate.score(prompt, response)
                σ_after.append(float(σ_new))
        b = sum(σ_before)
        a = sum(σ_after)
        return {
            "examples": len(rows),
            "σ_before_avg": b / max(len(σ_before), 1),
            "σ_after_avg": a / max(len(σ_after), 1),
            "improved": a < b,
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
