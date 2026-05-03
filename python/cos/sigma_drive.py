# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v178 σ-drive: intrinsic motivation shaped by σ — curiosity, competence, homeostasis (lab).

**Not affective AI:** VAD tuple is a logging prior, not measured emotion.
"""
from __future__ import annotations

from typing import Any, Dict, List, Sequence, Tuple


def sigma_gradient_emotion(hist: Sequence[float], *, dt: float = 1.0) -> Dict[str, Any]:
    """Finite-difference trend on recent σ → coarse label (not clinical affect)."""
    h = [float(x) for x in hist]
    if len(h) < 2:
        return {"d_sigma_dt": 0.0, "label": "flat"}
    ds = (h[-1] - h[-2]) / max(float(dt), 1e-9)
    if ds > 0.02:
        label = "stress_rising"
    elif ds < -0.02:
        label = "stress_falling"
    else:
        label = "steady"
    return {"d_sigma_dt": float(ds), "label": label}


class DriveGateView:
    """Augments any ``score(prompt, response)`` gate with σ statistics for drive math."""

    def __init__(self, inner: Any) -> None:
        self._inner = inner
        self._sigma_hist: List[float] = []

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        s, v = self._inner.score(prompt, response)
        self._sigma_hist.append(float(s))
        return float(s), str(v)

    def avg_sigma(self) -> float:
        if not self._sigma_hist:
            return 0.5
        tail = self._sigma_hist[-32:]
        return sum(tail) / max(len(tail), 1)

    def estimate_transition_sigma(self, obs: Any, action: Any) -> float:
        s, _ = self.score(str(obs), f"predict_next_after:{action}")
        return float(s)

    def get_state(self) -> Dict[str, Any]:
        return {"ema": self.avg_sigma(), "n_samples": len(self._sigma_hist)}


class SigmaCuriosity:
    def __init__(self, gate: Any) -> None:
        self.gate = gate
        self.prediction_errors: List[float] = []

    def reward(self, obs: Any, action: Any, next_obs: Any) -> float:
        pred_fn = getattr(self.gate, "estimate_transition_sigma", None)
        if callable(pred_fn):
            predicted_sigma = float(pred_fn(obs, action))
        else:
            predicted_sigma, _ = self.gate.score(str(obs), f"transition:{action}")
        actual_sigma, _ = self.gate.score(str(obs), str(next_obs))
        error = abs(predicted_sigma - float(actual_sigma))
        self.prediction_errors.append(error)
        if error < 0.1:
            return 0.0
        if error > 0.8:
            return -0.5
        return float(error)

    def level(self) -> float:
        if not self.prediction_errors:
            return 0.5
        tail = self.prediction_errors[-10:]
        return sum(tail) / max(len(tail), 1)


class SigmaCompetence:
    def __init__(self, gate: Any) -> None:
        self.gate = gate
        self.sigma_history: List[float] = []

    def reward(self, obs: Any, next_obs: Any) -> float:
        sigma, _ = self.gate.score(str(obs), str(next_obs))
        self.sigma_history.append(float(sigma))
        if len(self.sigma_history) < 2:
            return 0.0
        delta = self.sigma_history[-2] - self.sigma_history[-1]
        return max(0.0, delta * 5.0)

    def level(self) -> float:
        if not self.sigma_history:
            return 0.5
        tail = self.sigma_history[-10:]
        return 1.0 - sum(tail) / max(len(tail), 1)


class SigmaHomeostasis:
    def __init__(self, gate: Any, optimal_low: float = 0.1, optimal_high: float = 0.3) -> None:
        self.gate = gate
        self.optimal_low = float(optimal_low)
        self.optimal_high = float(optimal_high)

    def reward(self) -> float:
        avg_fn = getattr(self.gate, "avg_sigma", None)
        if not callable(avg_fn):
            return 0.0
        avg_sigma = float(avg_fn())
        if self.optimal_low <= avg_sigma <= self.optimal_high:
            return 1.0
        if avg_sigma < self.optimal_low:
            return 0.5
        return -abs(avg_sigma - self.optimal_high)


class SigmaDrive:
    def __init__(self, gate: Any) -> None:
        self._view = gate if hasattr(gate, "avg_sigma") else DriveGateView(gate)
        self.gate = self._view
        self.drives: Dict[str, Any] = {
            "curiosity": SigmaCuriosity(self._view),
            "competence": SigmaCompetence(self._view),
            "homeostasis": SigmaHomeostasis(self._view),
        }
        self.emotional_state: Dict[str, float] = {
            "valence": 0.0,
            "arousal": 0.5,
            "dominance": 0.5,
        }

    def intrinsic_reward(self, observation: Any, action: Any, next_observation: Any) -> Dict[str, Any]:
        curiosity_reward = float(self.drives["curiosity"].reward(observation, action, next_observation))
        competence_reward = float(self.drives["competence"].reward(observation, next_observation))
        homeostasis_reward = float(self.drives["homeostasis"].reward())
        total = 0.4 * curiosity_reward + 0.4 * competence_reward + 0.2 * homeostasis_reward
        self.update_emotion(total)
        return {
            "total_reward": total,
            "curiosity": curiosity_reward,
            "competence": competence_reward,
            "homeostasis": homeostasis_reward,
            "emotional_state": dict(self.emotional_state),
        }

    def update_emotion(self, reward: float) -> None:
        self.emotional_state["valence"] = 0.9 * self.emotional_state["valence"] + 0.1 * float(reward)
        change = abs(float(reward))
        self.emotional_state["arousal"] = 0.9 * self.emotional_state["arousal"] + 0.1 * change
        avg_s = self._view.avg_sigma() if hasattr(self._view, "avg_sigma") else 0.5
        self.emotional_state["dominance"] = max(0.0, min(1.0, 1.0 - float(avg_s)))

    def should_explore(self) -> bool:
        curiosity = float(self.drives["curiosity"].level())
        competence = float(self.drives["competence"].level())
        if curiosity > 0.7 and competence < 0.5:
            return True
        return False

    def emotional_gradient(self) -> Dict[str, Any]:
        hist = list(getattr(self._view, "_sigma_hist", [])[-32:])
        return sigma_gradient_emotion(hist)


__all__ = [
    "DriveGateView",
    "SigmaCompetence",
    "SigmaCuriosity",
    "SigmaDrive",
    "SigmaHomeostasis",
    "sigma_gradient_emotion",
]
