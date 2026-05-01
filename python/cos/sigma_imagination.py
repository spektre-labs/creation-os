# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-imagination (lab): multi-step latent roll-outs with a rising uncertainty prior and the
canonical ``sigma_gate_core`` interrupt between steps.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from cos.sigma_jepa import K_RAW_DEFAULT, SigmaJEPA


class SigmaImagination:
    def __init__(self, jepa: SigmaJEPA, *, k_raw: float = K_RAW_DEFAULT) -> None:
        self.jepa = jepa
        self.k_raw = float(k_raw)

    def estimate_rollout_sigma(
        self, z_current: Sequence[float], z_next: Sequence[float], step: int
    ) -> float:
        """Roll-out prior: rises with depth so the gate tends to ABSTAIN mid-horizon (lab)."""
        base = self.jepa.predictor.uncertainty(z_current, z_next)
        ramp = min(0.999, 0.05 + 0.12 * float(step))
        growth = 1.0 + 0.06 * float(step)
        mixed = ramp * 0.92 + 6.0 * float(base)
        return min(0.999, max(mixed, min(0.999, base * growth)))

    def imagine(
        self,
        current_observation: Any,
        action_sequence: Sequence[str],
        *,
        max_steps: int = 10,
    ) -> Dict[str, Any]:
        z = self.jepa.encode(current_observation)
        trajectory: List[Dict[str, Any]] = []
        cumulative_sigma = 0.0
        st = SigmaState()

        for step, action in enumerate(list(action_sequence)[: int(max_steps)]):
            z_next = self.jepa.predict(z, action)
            step_sigma = self.estimate_rollout_sigma(z, z_next, step)
            cumulative_sigma = max(cumulative_sigma, step_sigma)
            sigma_update(st, cumulative_sigma, self.k_raw)
            verdict = sigma_gate(st)
            trajectory.append(
                {
                    "step": step,
                    "sigma": float(step_sigma),
                    "cumulative_sigma": float(cumulative_sigma),
                    "verdict": verdict.name,
                }
            )
            if verdict == Verdict.ABSTAIN:
                break
            z = z_next

        if trajectory and trajectory[-1]["verdict"] == Verdict.ABSTAIN.name:
            reliable_horizon = max(0, len(trajectory) - 1)
        else:
            reliable_horizon = len(trajectory)

        return {
            "trajectory": trajectory,
            "reliable_horizon": reliable_horizon,
            "final_sigma": float(cumulative_sigma),
        }

    def plan(
        self,
        observation: Any,
        candidate_actions: Sequence[Sequence[str]],
        *,
        horizon: int = 5,
    ) -> Tuple[Optional[Tuple[str, ...]], float]:
        best_plan: Optional[Tuple[str, ...]] = None
        best_sigma = float("inf")
        for actions in candidate_actions:
            seq = list(actions)[: int(horizon)]
            result = self.imagine(observation, seq, max_steps=int(horizon))
            fs = float(result["final_sigma"])
            if fs < best_sigma:
                best_sigma = fs
                best_plan = tuple(seq)
        return best_plan, best_sigma


__all__ = ["SigmaImagination"]
