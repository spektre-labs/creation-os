# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-recursion — lab **mixture-of-recursions** hook (per-token depth routing).

``AdaptiveDepth`` in :mod:`cos.depth` allocates depth for a **whole** scored answer;
this module assigns a recurrence depth **per token** from the same lite entropy signal
as :class:`cos.stream.SigmaStream` (prompt + prefix up to each token), without claiming
NeurIPS MoR training or measured KV reductions. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence

__all__ = ["SigmaRecursion"]


class SigmaRecursion:
    """Per-token recursion depth from σ (gate entropy); KV sketch + halting helper."""

    def __init__(
        self,
        gate: Any = None,
        *,
        min_depth: int = 1,
        max_depth: int = 8,
        halt_epsilon: float = 0.02,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.min_depth = max(1, int(min_depth))
        self.max_depth = max(self.min_depth, int(max_depth))
        self.halt_epsilon = float(halt_epsilon)

    def _sigma_to_depth(self, sigma: float) -> int:
        """Lower σ (higher entropy / harder prefix) → more recursive steps."""
        s = max(0.0, min(1.0, float(sigma)))
        span = self.max_depth - self.min_depth
        extra = int(round((1.0 - s) * span))
        return self.min_depth + extra

    def token_route(
        self,
        tokens: Sequence[str],
        gate: Any,
        *,
        context: str = "",
    ) -> Dict[str, Any]:
        """Score each cumulative prefix; return per-token σ and integer depth map.

        Uses :meth:`~cos.sigma_gate.SigmaGate.compute_sigma` so lite mode does not
        mutate the gate EMA (deterministic for fixed ``context`` + ``tokens``).
        """
        g = gate
        ctx = str(context)
        depths: List[int] = []
        sigmas: List[float] = []
        per_token: List[Dict[str, Any]] = []
        prefix = ""
        for idx, tok in enumerate(tokens):
            prefix = prefix + str(tok)
            sigma = float(g.compute_sigma(None, None, ctx, prefix))
            verdict = str(g._verdict(sigma))
            depth = self._sigma_to_depth(sigma)
            depths.append(depth)
            sigmas.append(sigma)
            per_token.append(
                {
                    "index": idx,
                    "token": str(tok),
                    "sigma": round(sigma, 6),
                    "verdict": verdict,
                    "depth": depth,
                }
            )
        return {
            "tokens": list(tokens),
            "depths": depths,
            "sigmas": sigmas,
            "per_token": per_token,
            "max_depth": max(depths) if depths else self.min_depth,
            "mean_depth": sum(depths) / max(len(depths), 1),
        }

    def recursive_forward(
        self,
        tokens: Sequence[str],
        block: Callable[[str], str],
        depths: Sequence[int],
    ) -> List[str]:
        """Apply ``block`` ``depths[i]`` times to each token (lab surrogate for blocks)."""
        if len(depths) != len(tokens):
            raise ValueError("depths must match tokens length")
        out: List[str] = []
        for tok, d in zip(tokens, depths):
            x = str(tok)
            for _ in range(int(d)):
                x = block(x)
            out.append(x)
        return out

    def selective_kv_cache(
        self,
        tokens: Sequence[str],
        depths: Sequence[int],
        *,
        max_depth: Optional[int] = None,
    ) -> Dict[str, Any]:
        """Upper-bound sketch: sum of per-token depths vs ``len(tokens) * cap``."""
        if len(depths) != len(tokens):
            raise ValueError("depths must match tokens length")
        if max_depth is not None:
            cap = int(max_depth)
        elif depths:
            cap = max(int(d) for d in depths)
        else:
            cap = 1
        cap = max(1, cap)
        full = len(tokens) * cap
        active = sum(min(int(d), cap) for d in depths)
        return {
            "full_kv_slots": full,
            "active_kv_slots": active,
            "fraction": active / max(full, 1),
            "cap": cap,
        }

    def compute_savings(
        self,
        tokens: Sequence[str],
        depths: Sequence[int],
        *,
        max_depth: Optional[int] = None,
    ) -> Dict[str, Any]:
        """Fraction of KV upper-bound not used when running only assigned depths."""
        kv = self.selective_kv_cache(tokens, depths, max_depth=max_depth)
        full = kv["full_kv_slots"]
        active = kv["active_kv_slots"]
        savings = (full - active) / max(full, 1)
        return {
            "savings_ratio": round(savings, 6),
            "full_kv_slots": full,
            "active_kv_slots": active,
        }

    def halt_on_sigma_stable(
        self,
        prompt: str,
        seed: str,
        gate: Any,
        block: Callable[[str], str],
        *,
        max_rounds: int = 16,
    ) -> Dict[str, Any]:
        """Refine ``seed`` with ``block``; stop when |Δσ| < ``halt_epsilon`` across rounds."""
        g = gate
        text = str(seed)
        trace: List[Dict[str, Any]] = []
        prev_sigma: Optional[float] = None
        for rnd in range(max_rounds):
            text = block(text)
            sigma = float(g.compute_sigma(None, None, str(prompt), text))
            trace.append({"round": rnd, "sigma": round(sigma, 6), "text_len": len(text)})
            if prev_sigma is not None and abs(sigma - prev_sigma) < self.halt_epsilon:
                return {
                    "halted": True,
                    "rounds": rnd + 1,
                    "final_sigma": sigma,
                    "trace": trace,
                }
            prev_sigma = sigma
        return {"halted": False, "rounds": max_rounds, "final_sigma": prev_sigma, "trace": trace}
