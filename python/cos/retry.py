# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-retry — strategies, σ trends, budget, circuit breaker, backoff (lab).

Use from :class:`cos.workflow.SigmaWorkflow` on RETHINK paths."""
from __future__ import annotations

import time
from typing import Any, Callable, Dict, List, Mapping, Sequence, Tuple

__all__ = ["SigmaRetry"]

Strategy = str  # same | rephrase | escalate | decompose

Fn = Callable[..., Any]


class SigmaRetry:
    """Wrap a callable; adapt prompt/context per strategy; gate σ after each attempt."""

    STRATEGIES = frozenset({"same", "rephrase", "escalate", "decompose"})

    @staticmethod
    def backoff(attempt_number: int, *, base_s: float = 0.05, cap_s: float = 2.0) -> float:
        n = max(1, int(attempt_number))
        return min(float(cap_s), float(base_s) * (2 ** (n - 1)))

    @staticmethod
    def circuit_breaker(consecutive_failures: int, threshold: int) -> Dict[str, Any]:
        tripped = int(consecutive_failures) >= int(threshold)
        return {"open": tripped, "failures": int(consecutive_failures), "threshold": int(threshold)}

    @staticmethod
    def budget_aware(remaining_eur: float, attempt_cost: float) -> Dict[str, Any]:
        rc = float(remaining_eur)
        ac = float(attempt_cost)
        return {"can_retry": ac <= rc + 1e-9, "remaining_eur": rc, "attempt_cost": ac}

    @staticmethod
    def sigma_trend(attempts: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        sigs = [float(a.get("sigma", 0.5)) for a in attempts]
        if len(sigs) < 2:
            return {"improving": True, "delta_first_last": 0.0}
        improving = sigs[-1] < sigs[0]
        return {
            "improving": improving,
            "delta_first_last": round(sigs[-1] - sigs[0], 6),
        }

    @staticmethod
    def _apply_strategy(
        strategy: Strategy,
        fn: Fn,
        ctx: Dict[str, Any],
        *,
        attempt: int,
    ) -> Dict[str, Any]:
        s = str(strategy).lower()
        c = dict(ctx)
        if s == "same":
            return dict(fn(c))
        if s == "rephrase":
            c["prompt"] = str(c.get("prompt", "")) + f" [retry:{attempt}]"
            return dict(fn(c))
        if s == "escalate":
            c["model_tier"] = max(int(c.get("model_tier", 0)), attempt)
            return dict(fn(c))
        if s == "decompose":
            sub = str(c.get("prompt", "")).strip()
            c["prompt"] = f"Part {attempt}: {sub[:400]}"
            return dict(fn(c))
        raise ValueError(f"unknown strategy {strategy!r}")

    @classmethod
    def retry(
        cls,
        fn: Fn,
        gate: Any,
        max_attempts: int,
        strategy: Strategy,
        ctx: Dict[str, Any],
        *,
        score_keys: Tuple[str, str] = ("prompt", "response"),
    ) -> Dict[str, Any]:
        max_attempts = max(1, int(max_attempts))
        if str(strategy).lower() not in cls.STRATEGIES:
            raise ValueError(f"strategy must be one of {cls.STRATEGIES}")
        attempts: List[Dict[str, Any]] = []
        last: Dict[str, Any] = {}
        for i in range(1, max_attempts + 1):
            if i > 1:
                time.sleep(cls.backoff(i - 1))
            last = cls._apply_strategy(str(strategy), fn, ctx, attempt=i)
            p, r = str(last.get(score_keys[0], "")), str(last.get(score_keys[1], ""))
            sigma, verdict = gate.score(p, r)
            rec = {"attempt": i, "sigma": float(sigma), "verdict": str(verdict), "state": last}
            attempts.append(rec)
            ctx.update(last)
            if str(verdict).upper() == "ACCEPT":
                break
        trend = cls.sigma_trend(attempts)
        return {"final": last, "attempts": attempts, "trend": trend}

