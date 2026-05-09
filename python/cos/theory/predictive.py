# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Hierarchical **predictive coding** sketch: σ as top-down vs bottom-up **mismatch**.

Neuroscience-style predictive coding posits downward predictions and upward **prediction
errors**. Here :class:`~cos.sigma_gate.SigmaGate` scores ``prediction`` vs ``observation`` —
that σ **stands in for** an error channel in this **discrete** lab story.

This is **not** a spiking neural implementation, **not** a proof about L0–L9 silicon, and
**not** the complete “Friston stack” by itself — pair with :mod:`cos.theory.factor_graph` and
:mod:`cos.theory.active_inference` only as **composable metaphors**, not as a single validated brain
model. **Zero** extra dependencies. **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["PredictiveLayer", "PredictiveCoding"]


class PredictiveLayer:
    """One level: top-down ``prediction``, bottom-up ``observation``, σ = gate mismatch."""

    def __init__(self, name: str, level: int, gate: Any) -> None:
        self.name = str(name)
        self.level = int(level)
        self.gate = gate
        self.prediction: Any = None
        self.observation: Any = None
        self.error: float = 0.0
        self.precision: float = 1.0

    def predict(self, state: Any) -> Any:
        """Cache top-down prediction."""
        self.prediction = state
        return state

    def observe(self, input_data: Any) -> float:
        """Bottom-up datum; σ = gate(prediction, observation)."""
        self.observation = input_data
        if self.prediction is not None:
            sigma, _v = self.gate.score(str(self.prediction), str(self.observation))
            self.error = float(sigma) * float(self.precision)
        else:
            self.error = 0.5
        return float(self.error)

    def update(self, learning_rate: float = 0.1) -> Dict[str, Any]:
        """Toy prediction nudge: concatenate prediction with observation; re-score."""
        _ = learning_rate
        if self.prediction is None or self.observation is None:
            return {"updated": False, "layer": self.name}
        err_before = float(self.error)
        self.prediction = f"{self.prediction}|{self.observation}"
        new_sigma, _v = self.gate.score(str(self.prediction), str(self.observation))
        new_err = float(new_sigma) * float(self.precision)
        improved = new_err < err_before
        self.error = new_err
        return {
            "updated": True,
            "layer": self.name,
            "error_before": round(err_before, 4),
            "error_after": round(new_err, 4),
            "improved": improved,
        }


class PredictiveCoding:
    """Ordered stack of :class:`PredictiveLayer`; bottom-up error summary + local updates."""

    def __init__(self, gate: Any = None, n_levels: int = 5) -> None:
        self.gate = gate or SigmaGate()
        n = max(1, int(n_levels))
        self.layers: List[PredictiveLayer] = [
            PredictiveLayer(f"L{i}", i, self.gate) for i in range(n)
        ]

    def top_down_pass(self, high_level_state: Any) -> None:
        """Push coarse state downward (reversed layer order = high → low)."""
        state = high_level_state
        for layer in reversed(self.layers):
            layer.predict(state)
            state = f"predicted_by_{layer.name}: {state}"

    def bottom_up_pass(self, sensory_input: Any) -> List[Dict[str, Any]]:
        """Drive each layer with an ascending error-tagged carrier."""
        errors: List[Dict[str, Any]] = []
        signal = sensory_input
        for layer in self.layers:
            error = layer.observe(signal)
            errors.append(
                {
                    "layer": layer.name,
                    "σ": round(float(error), 4),
                    "sigma": round(float(error), 4),
                    "precision": round(float(layer.precision), 4),
                }
            )
            signal = f"error_{layer.name}={error:.3f}"
        return errors

    def full_cycle(self, sensory_input: Any, high_level_state: Optional[Any] = None) -> Dict[str, Any]:
        """Top-down (optional) → bottom-up errors → per-layer ``update``."""
        if high_level_state is not None:
            self.top_down_pass(high_level_state)

        errors = self.bottom_up_pass(sensory_input)
        updates: List[Dict[str, Any]] = []
        for layer in self.layers:
            updates.append(layer.update())

        total_σ = sum(float(e["σ"]) for e in errors) / max(len(errors), 1)
        return {
            "errors": errors,
            "updates": updates,
            "total_σ": round(total_σ, 4),
            "layers": len(self.layers),
        }

    def run(
        self,
        inputs: Sequence[Any],
        n_iterations: int = 5,
        high_level_state: Optional[Any] = None,
    ) -> Dict[str, Any]:
        """Repeated ``full_cycle`` per input.

        If ``high_level_state`` is set, :meth:`top_down_pass` is applied **once** before
        the inner loops so local ``update`` blends are not erased on every iteration.
        """
        if high_level_state is not None:
            self.top_down_pass(high_level_state)
        history: List[Dict[str, Any]] = []
        last: Dict[str, Any] = {}
        for inp in inputs:
            for _ in range(max(1, int(n_iterations))):
                last = self.full_cycle(inp, None)
            history.append({"input": str(inp)[:50], "final_σ": last.get("total_σ", 0.5)})

        traj = [h["final_σ"] for h in history]
        learned = len(history) > 1 and history[-1]["final_σ"] < history[0]["final_σ"]
        return {
            "history": history,
            "σ_trajectory": traj,
            "sigma_trajectory": traj,
            "learned": learned,
        }

    def precision_weight(self, layer_index: int, precision: float) -> None:
        """Set multiplicative precision weight on a layer’s σ."""
        if 0 <= int(layer_index) < len(self.layers):
            self.layers[int(layer_index)].precision = float(precision)
