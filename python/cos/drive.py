# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-gradient as a drive signal: curiosity vs anxiety from coherence dynamics.

:class:`SigmaDriveV2` adds richer σ-trajectory labels + intrinsic ``explore`` scoring (lab only;
**NOT** clinical affect or AGI emotion). See ``docs/CLAIM_DISCIPLINE.md``."""

from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

from typing import Any, Dict, List

__all__ = ["SigmaDrive", "SigmaDriveV2"]


class SigmaDrive:
    """Internal drive from recent σ only (no external reward)."""

    def __init__(self) -> None:
        self.sigma_history: List[float] = []
        self.emotion_log: List[Dict[str, Any]] = []

    @property
    def σ_history(self) -> List[float]:
        """Alias for ``sigma_history`` (σ-only drive buffer)."""
        return self.sigma_history

    def record(self, sigma: float) -> None:
        self.sigma_history.append(float(sigma))

    def gradient(self, window: int = 5) -> float:
        """Finite-difference trend: (last − first) / span over the last ``window`` points."""
        if len(self.sigma_history) < 2:
            return 0.0
        w = max(1, int(window))
        recent = self.sigma_history[-w:]
        if len(recent) < 2:
            return 0.0
        span = len(recent)
        return float((recent[-1] - recent[0]) / span)

    def emotion(self) -> Dict[str, Any]:
        if len(self.sigma_history) < 2:
            return {"state": "neutral", "σ": 0.0, "gradient": 0.0}

        sigma = float(self.sigma_history[-1])
        grad = self.gradient()

        if grad < -0.02:
            state = "curiosity"
        elif grad > 0.02:
            state = "anxiety"
        elif sigma < 0.1:
            state = "satisfaction"
        elif sigma > 0.5 and abs(grad) < 0.01:
            state = "frustration"
        else:
            state = "neutral"

        result = {"state": state, "σ": round(sigma, 4), "gradient": round(grad, 6)}
        self.emotion_log.append(result)
        return result

    def should_continue(self) -> bool:
        e = self.emotion()
        st = e["state"]
        if st == "curiosity":
            return True
        if st == "satisfaction":
            return False
        if st == "frustration":
            return False
        if st == "anxiety":
            return False
        return True


class SigmaDriveV2:
    """Lab drive from σ time series + optional gate for exploration probe (measurement hooks)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()
        self.σ_history: List[float] = []
        self.exploration_history: List[Dict[str, Any]] = []

    def record(self, σ: float) -> None:
        self.σ_history.append(float(σ))

    def emotion(self) -> Dict[str, Any]:
        if len(self.σ_history) < 3:
            return {"state": "neutral", "σ": 0.5, "Δσ": 0.0, "volatility": 0.0}

        recent = self.σ_history[-5:]
        current = float(recent[-1])
        trend = float(recent[-1] - recent[0])
        denom = max(len(recent) - 1, 1)
        volatility = sum(abs(float(recent[i + 1]) - float(recent[i])) for i in range(len(recent) - 1)) / denom

        if trend < -0.1 and current < 0.3:
            state = "flow"
        elif trend < -0.05:
            state = "curiosity"
        elif abs(trend) < 0.02 and current < 0.2:
            state = "contentment"
        elif abs(trend) < 0.02 and current > 0.5:
            state = "boredom"
        elif trend > 0.1:
            state = "anxiety"
        elif volatility > 0.15:
            state = "confusion"
        elif trend > 0.05 and current > 0.7:
            state = "frustration"
        else:
            state = "engagement"

        return {
            "state": state,
            "σ": round(current, 4),
            "Δσ": round(trend, 4),
            "volatility": round(volatility, 4),
        }

    def curiosity_reward(self, observation: Any) -> Dict[str, Any]:
        σ_before = self.σ_history[-1] if self.σ_history else 0.5
        σ_after, _ = self.gate.score("explore", str(observation))
        sa = float(σ_after)
        reward = max(0.0, float(σ_before) - sa)
        self.record(sa)
        row = {
            "observation": str(observation)[:50],
            "σ_before": round(float(σ_before), 4),
            "σ_after": round(sa, 4),
            "reward": round(reward, 4),
            "worth_exploring": reward > 0.05,
        }
        self.exploration_history.append(row)
        return row

    def boredom_signal(self) -> Dict[str, Any]:
        if len(self.σ_history) < 10:
            return {"bored": False, "σ_variance": None, "σ_avg": None, "action": "continue current"}

        recent = self.σ_history[-10:]
        mean = sum(float(s) for s in recent) / len(recent)
        variance = sum((float(s) - mean) ** 2 for s in recent) / len(recent)
        bored = variance < 0.001 and mean > 0.3

        return {
            "bored": bored,
            "σ_variance": round(float(variance), 6),
            "σ_avg": round(float(mean), 4),
            "action": "explore new domain" if bored else "continue current",
        }

    def flow_detector(self) -> Dict[str, Any]:
        em = self.emotion()
        return {
            "in_flow": em["state"] == "flow",
            "conditions": {
                "σ_low": em["σ"] < 0.3,
                "improving": em["Δσ"] < 0,
                "stable": em["volatility"] < 0.1,
            },
            "state": em["state"],
        }

    def should_explore_or_exploit(self) -> Dict[str, Any]:
        em = self.emotion()
        st = em["state"]

        if st in ("boredom", "frustration"):
            return {"decision": "EXPLORE", "reason": st}
        if st in ("flow", "contentment"):
            return {"decision": "EXPLOIT", "reason": st}
        if st == "curiosity":
            return {"decision": "EXPLORE_FOCUSED", "reason": "learning active"}
        if st == "anxiety":
            return {"decision": "RETREAT", "reason": "σ rising — simplify"}
        return {"decision": "CONTINUE", "reason": st}
