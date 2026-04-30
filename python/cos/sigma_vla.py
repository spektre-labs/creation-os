# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
**σ-VLA** scaffold: fuse perception σ (via :class:`SigmaSense`) with an action-confidence
scalar, then run the **same** Q16 interrupt mirror as text stacks (``sigma_gate_core``).

**Lab scaffold:** ``policy``, ``compute_action_sigma``, and ``request_human_input`` are
injectable. A **single** ``sigma_update`` from a cold ``SigmaState`` is used so typical
non-zero σ maps to ``RETHINK`` (escalate / replan) while extreme σ can ``ABSTAIN`` —
matching VLA "do not move unless confident" sketches.

See ``docs/CLAIM_DISCIPLINE.md`` — no sim / hardware headline without harness JSON.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any, Callable, Optional, Tuple

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from .sigma_sense import SigmaSense


def _verdict_from_sigma(sigma_01: float, k_raw: float) -> Verdict:
    """One interrupt step from a cold ``SigmaState`` (positive σ ⇒ ``RETHINK`` unless ABSTAIN)."""
    st = SigmaState()
    s = max(0.0, min(1.0, float(sigma_01)))
    kr = max(0.0, min(1.0, float(k_raw)))
    sigma_update(st, s, kr)
    return sigma_gate(st)


@dataclass
class SigmaVLA:
    """Vision-language-action policy with σ-gated execution (fail closed on ABSTAIN)."""

    sense: SigmaSense
    policy: Callable[[Any, str], Any]
    compute_action_sigma: Callable[[Any], float]
    request_human_input: Callable[[Any, str], Any]
    k_raw: float = 0.92

    def act(self, observation: Any, instruction: str) -> Tuple[Optional[Any], Verdict]:
        """
        Return ``(action_or_fallback_or_none, verdict)``.

        * ``ACCEPT`` → planned ``action``.
        * ``RETHINK`` → ``request_human_input(observation, instruction)``.
        * ``ABSTAIN`` → ``(None, ABSTAIN)`` (do not move / emit actuators).
        """
        ins = str(instruction or "").strip()
        perception_sigma, _modal = self.sense.perceive({"image": observation, "text": ins})
        perception_sigma = max(0.0, min(1.0, float(perception_sigma)))

        action = self.policy(observation, ins)
        action_sigma = max(0.0, min(1.0, float(self.compute_action_sigma(action))))
        total_sigma = max(perception_sigma, action_sigma)

        verdict = _verdict_from_sigma(total_sigma, self.k_raw)
        if verdict == Verdict.ACCEPT:
            return action, verdict
        if verdict == Verdict.RETHINK:
            return self.request_human_input(observation, ins), verdict
        return None, verdict


__all__ = ["SigmaVLA"]
