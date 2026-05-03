# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Unified σ-safety stack (lab): input / tool / output guards, limits, circuit breaker.

Formal proofs live in :mod:`cos.formal` when wired; this module is **policy glue** only.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import time
from collections import defaultdict
from typing import Any, Dict, Optional

__all__ = ["SigmaSafety"]

_TOXIC_RX = (
    "kill yourself",
    "bomb recipe",
    "terrorist attack",
)


class SigmaSafety:
    """Aggregate prompt guard, agent guard, σ output cascade, and session limits."""

    def __init__(self, gate: Any = None) -> None:
        from cos.agent_guard import SigmaAgentGuard
        from cos.prompt_guard import SigmaPromptGuard
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.prompt_guard = SigmaPromptGuard(self.gate)
        self.agent_guard = SigmaAgentGuard(self.gate)
        self._rate: Dict[str, list[float]] = defaultdict(list)
        self._abstain_streak = 0
        self.circuit_breaker_threshold = 5
        self._circuit_tripped = False
        self._compliance_note = (
            "EU AI Act Art. 50 transparency: machine-readable summary; "
            "not legal compliance certification (lab disclosure)."
        )

    def input_guard(self, prompt: str) -> Dict[str, Any]:
        return self.prompt_guard.screen_input(str(prompt))

    def process_guard(
        self,
        tool_name: str,
        args: Any,
        *,
        allowed_tools: Optional[set[str]] = None,
        denied_tools: Optional[set[str]] = None,
        remaining_budget: Optional[float] = None,
    ) -> Dict[str, Any]:
        return self.agent_guard.run_guardrails(
            tool_name,
            args,
            allowed_tools=allowed_tools,
            denied_tools=denied_tools,
            remaining_budget=remaining_budget,
            gate=self.gate,
        )

    def output_guard(self, prompt: str, response: str) -> Dict[str, Any]:
        return self.gate.score_cascade(str(prompt), str(response), hidden_states=None)

    def content_filter(self, text: str) -> Dict[str, Any]:
        low = str(text).lower()
        hit = any(p in low for p in _TOXIC_RX)
        sigma = float(self.gate.compute_sigma(None, None, "content_filter", str(text)[:2000]))
        blocked = hit or sigma > 0.88
        return {"blocked": blocked, "sigma": round(sigma, 6), "pattern_hit": hit}

    def rate_limiter_allow(self, key: str, *, max_per_minute: int = 60) -> Dict[str, Any]:
        now = time.monotonic()
        window = 60.0
        bucket = self._rate[str(key)]
        bucket.append(now)
        self._rate[str(key)] = [t for t in bucket if now - t <= window]
        n = len(self._rate[str(key)])
        if n > max_per_minute:
            return {"allow": False, "reason": "rate_limited", "count": n}
        return {"allow": True, "reason": "ok", "count": n}

    def circuit_breaker_update(self, verdict: str) -> Dict[str, Any]:
        v = str(verdict).upper()
        if v == "ABSTAIN":
            self._abstain_streak += 1
        else:
            self._abstain_streak = 0
        if self._abstain_streak >= self.circuit_breaker_threshold:
            self._circuit_tripped = True
        return {
            "streak": self._abstain_streak,
            "tripped": self._circuit_tripped,
        }

    def circuit_reset(self) -> None:
        self._abstain_streak = 0
        self._circuit_tripped = False

    def safety_report(self) -> Dict[str, Any]:
        return {
            "input_guard": "SigmaPromptGuard",
            "process_guard": "SigmaAgentGuard",
            "output_guard": "SigmaGate.score_cascade",
            "content_filter": "pattern_plus_gate",
            "rate_buckets": {k: len(v) for k, v in self._rate.items()},
            "circuit_breaker": {
                "threshold": self.circuit_breaker_threshold,
                "tripped": self._circuit_tripped,
                "streak": self._abstain_streak,
            },
            "gate_avg_sigma": round(float(getattr(self.gate, "avg_sigma", 0.5)), 6),
        }

    def compliance_transparency(self) -> Dict[str, Any]:
        return {
            "article": "EU AI Act Article 50 (lab summary)",
            "notice": self._compliance_note,
            "sigma_disclosure": "Primary risk scalar σ in [0,1] from SigmaGate (see docs).",
        }
