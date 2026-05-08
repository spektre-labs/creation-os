# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-TTT — test-time adaptation **lab** (scalar ``fast_weights``, optional σ-gated multi-step refine).

v1: nudge ``fast_weights`` dict + labeled-example threshold tuning (no real backward pass).
v2: :meth:`process` runs extra refinement steps **only** when σ exceeds a trigger (saves compute).

No blanket “beats transformers” claims; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple, Union, cast

__all__ = ["SigmaTTT"]


def _norm_verdict(verdict: Any) -> str:
    raw = str(getattr(verdict, "name", verdict))
    return raw.split(".")[-1].upper() if "." in raw else raw.upper()


class SigmaTTT:
    """Trigger-gated nudge to ``fast_weights`` with rollback if σ worsens; optional σ-gated TTT steps."""

    def __init__(
        self,
        gate: Any = None,
        *,
        trigger_threshold: float = 0.45,
        trigger_σ: Optional[float] = None,
        max_steps: int = 5,
        learning_rate: float = 0.01,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        th = float(trigger_σ) if trigger_σ is not None else float(trigger_threshold)
        self.trigger_threshold = th
        self.max_steps = int(max_steps)
        self.lr = float(learning_rate)
        self.adaptations = 0
        self.skips = 0
        self.fast_weights: Dict[str, float] = {"mlp_proj": 1.0}
        # V2: prompt-prefix key → last adaptation telemetry (separate from legacy float weights)
        self.ttt_memory: Dict[str, Dict[str, Any]] = {}

    @property
    def trigger_σ(self) -> float:
        return float(self.trigger_threshold)

    def sigma_trigger(self, sigma: float) -> bool:
        return float(sigma) > self.trigger_threshold

    def process(
        self,
        prompt: str,
        response: str,
        adapt_fn: Optional[Callable[[str, str, int], str]] = None,
    ) -> Dict[str, Any]:
        """Score → if σ below trigger, skip; else refine up to ``max_steps`` (σ is the only trigger)."""
        σ_initial, verdict = self.gate.score(str(prompt), str(response))
        vn = _norm_verdict(verdict)

        if float(σ_initial) < self.trigger_threshold:
            self.skips += 1
            return {
                "response": response,
                "σ": round(float(σ_initial), 4),
                "verdict": vn,
                "adapted": False,
                "path": "FAST",
            }

        self.adaptations += 1
        adapted_response = str(response)
        σ_trajectory: List[float] = [float(σ_initial)]
        key = str(prompt)[:50]
        verdict_new: str = vn

        for step in range(self.max_steps):
            if adapt_fn is not None:
                adapted_response = adapt_fn(prompt, adapted_response, step)
            else:
                adapted_response = self._default_adapt(prompt, adapted_response, step)

            σ_new, verdict_raw = self.gate.score(str(prompt), adapted_response)
            verdict_new = _norm_verdict(verdict_raw)
            σ_trajectory.append(float(σ_new))

            self.ttt_memory[key] = {
                "σ_before": float(σ_initial),
                "σ_after": float(σ_new),
                "steps": step + 1,
            }

            if float(σ_new) < self.trigger_threshold:
                return {
                    "response": adapted_response,
                    "σ": round(float(σ_new), 4),
                    "verdict": verdict_new,
                    "adapted": True,
                    "path": "ADAPTED",
                    "steps": step + 1,
                    "σ_improvement": round(float(σ_initial) - float(σ_new), 4),
                    "trajectory": [round(float(s), 4) for s in σ_trajectory],
                }

        final_σ = float(σ_trajectory[-1])
        partial_verdict = "RETHINK" if final_σ < 0.7 else "ABSTAIN"
        return {
            "response": adapted_response,
            "σ": round(final_σ, 4),
            "verdict": partial_verdict,
            "adapted": True,
            "path": "ADAPTED_PARTIAL",
            "steps": self.max_steps,
            "σ_improvement": round(float(σ_initial) - final_σ, 4),
            "trajectory": [round(float(s), 4) for s in σ_trajectory],
        }

    def has_fast_weight(self, prompt: str) -> bool:
        return str(prompt)[:50] in self.ttt_memory

    def recall_fast_weight(self, prompt: str) -> Optional[Dict[str, Any]]:
        return self.ttt_memory.get(str(prompt)[:50])

    def _default_adapt(self, prompt: str, response: str, step: int) -> str:
        del prompt
        return f"{response} [adapted step {step + 1}]"

    def efficiency(self) -> Dict[str, Any]:
        total = self.adaptations + self.skips
        if total == 0:
            return {
                "skip_rate": 0.0,
                "compute_saved_steps": 0,
                "total_queries": 0,
                "adaptations": 0,
                "skips": 0,
                "fast_weights_learned": len(self.ttt_memory),
                "insight": "no queries yet",
            }

        skip_rate = self.skips / total
        compute_saved = self.skips * self.max_steps
        return {
            "total_queries": total,
            "adaptations": self.adaptations,
            "skips": self.skips,
            "skip_rate": round(skip_rate, 4),
            "compute_saved_steps": compute_saved,
            "fast_weights_learned": len(self.ttt_memory),
            "insight": (
                f"σ-gating saved {compute_saved} TTT steps "
                f"({skip_rate:.0%} of queries did not need adaptation)"
            ),
        }

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
