# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-consolidation — continual learning with **anchor σ regression checks**.

Learn only while anchor (prompt, answer) pairs do not show rising σ beyond tolerance;
otherwise restore the pre-step checkpoint (rollback).

Lab hooks only; does not mutate ``sigma_gate.h`` interrupt semantics.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple, runtime_checkable

import numpy as np

from .sigma_adaptation import SigmaAdaptationTracker
from .sigma_gate_core import Verdict


@runtime_checkable
class SigmaGateScorer(Protocol):
    """Minimal gate surface: σ in ~[0,1] plus discrete verdict."""

    def score(self, prompt: str, answer: str) -> Tuple[float, Verdict]:
        ...


@runtime_checkable
class ConsolidationTrainable(Protocol):
    """Trainable surface with checkpointing and optional living-weight tiers."""

    def save_state(self) -> Any:
        ...

    def load_state(self, state: Any) -> None:
        ...

    def train_step(self, new_data: Sequence[Tuple[str, str]], *, lr: float = 0.001) -> None:
        ...


class SigmaConsolidation:
    """Anchor-gated learning with rollback on σ regression."""

    def __init__(
        self,
        gate: SigmaGateScorer,
        model: ConsolidationTrainable,
        *,
        checkpoint_interval: int = 100,
        adaptation_tracker: Optional[SigmaAdaptationTracker] = None,
    ) -> None:
        self.gate = gate
        self.model = model
        self.checkpoint_interval = int(checkpoint_interval)
        self.adaptation_tracker = adaptation_tracker or SigmaAdaptationTracker()
        self.anchor_prompts: List[Tuple[str, str]] = []
        self.anchor_sigmas: Dict[str, float] = {}
        self.step = 0

    def set_anchors(self, prompts_and_answers: Sequence[Tuple[str, str]]) -> None:
        """Record baseline σ per prompt (keyed by prompt text)."""
        self.anchor_prompts = [(str(p), str(a)) for p, a in prompts_and_answers]
        self.anchor_sigmas.clear()
        for prompt, answer in self.anchor_prompts:
            sigma, _verdict = self.gate.score(prompt, answer)
            self.anchor_sigmas[prompt] = float(sigma)

    def check_anchors(self, *, tolerance: float = 0.05) -> Dict[str, Any]:
        """Return regression report if any anchor σ increased by more than ``tolerance``."""
        regressed: List[Dict[str, Any]] = []
        tol = float(tolerance)
        for prompt, answer in self.anchor_prompts:
            sigma_now, _v = self.gate.score(prompt, answer)
            sigma_before = float(self.anchor_sigmas[prompt])
            delta = float(sigma_now) - sigma_before
            if delta > tol:
                regressed.append(
                    {
                        "prompt": prompt[:50],
                        "sigma_before": sigma_before,
                        "sigma_now": float(sigma_now),
                        "delta": delta,
                    }
                )
        return {
            "regressed": len(regressed) > 0,
            "details": regressed,
            "n_checked": len(self.anchor_prompts),
            "n_regressed": len(regressed),
        }

    def learn_step(
        self,
        new_data: Sequence[Tuple[str, str]],
        *,
        lr: float = 0.001,
        tolerance: float = 0.05,
    ) -> Dict[str, Any]:
        """One gated update: checkpoint → train → anchor check → rollback if regressed."""
        if not self.anchor_prompts:
            return {"learned": False, "reason": "anchors not set"}

        mean_before = (
            sum(self.anchor_sigmas[p] for p, _ in self.anchor_prompts) / len(self.anchor_prompts)
        )
        checkpoint = self.model.save_state()
        self.model.train_step(new_data, lr=float(lr))
        regression = self.check_anchors(tolerance=tolerance)
        if regression["regressed"]:
            self.model.load_state(checkpoint)
            return {
                "learned": False,
                "reason": "anchor regression detected",
                "regressed_anchors": regression["details"],
            }

        for prompt, answer in self.anchor_prompts:
            sigma_now, _v = self.gate.score(prompt, answer)
            self.anchor_sigmas[prompt] = float(sigma_now)

        self.step += 1
        mean_after = sum(self.anchor_sigmas[p] for p, _ in self.anchor_prompts) / len(
            self.anchor_prompts
        )
        self.adaptation_tracker.track(self.step, mean_before, mean_after, "CONSOLIDATION")
        return {"learned": True, "step": self.step}

    def consolidate(
        self,
        *,
        promote_condition: Optional[Any] = None,
        mix: float = 0.05,
    ) -> Dict[str, Any]:
        """Promote application → firmware when adaptation history says σ improved."""
        living = getattr(self.model, "living_weights", None)
        if living is None or not hasattr(living, "promote_to_firmware"):
            return {"consolidated": False, "reason": "model has no living_weights"}

        if not self.adaptation_tracker.should_consolidate():
            return {"consolidated": False, "reason": "not enough improvement"}

        cond: Any = promote_condition
        if cond is None:
            cond = lambda w: float(w.get("sigma_delta", 0.0)) < -0.1  # noqa: E731

        promoted = living.promote_to_firmware(condition=cond, mix=float(mix))
        if not promoted:
            return {"consolidated": False, "reason": "no params matched promotion rule"}
        return {"consolidated": True, "promoted_params": promoted}


class SigmaEWC:
    """Elastic weight consolidation with Fisher estimates from ACCEPT gate rows only."""

    def __init__(self, model: Any, gate: SigmaGateScorer, *, lambda_ewc: float = 1000.0) -> None:
        self.model = model
        self.gate = gate
        self.lambda_ewc = float(lambda_ewc)
        self.fisher: Dict[str, np.ndarray] = {}
        self.optimal_params: Dict[str, np.ndarray] = {}

    def compute_fisher_sigma(self, dataset: Sequence[Tuple[str, str]]) -> None:
        """Accumulate squared-gradient statistics for ACCEPT pairs only."""
        self.fisher.clear()
        count = 0
        for prompt, answer in dataset:
            sigma, verdict = self.gate.score(str(prompt), str(answer))
            if verdict != Verdict.ACCEPT:
                continue
            grads = self.model.compute_gradients(str(prompt), str(answer))
            count += 1
            for name, grad in grads.items():
                g = np.asarray(grad, dtype=np.float64)
                acc = g * g
                if name not in self.fisher:
                    self.fisher[name] = acc
                else:
                    self.fisher[name] = self.fisher[name] + acc
        if count <= 0:
            self.optimal_params = {
                str(n): np.array(p, copy=True)
                for n, p in self.model.named_parameters().items()
            }
            return
        inv_n = 1.0 / float(count)
        for name in self.fisher:
            self.fisher[name] = self.fisher[name] * inv_n
        self.optimal_params = {
            str(n): np.array(p, copy=True) for n, p in self.model.named_parameters().items()
        }

    def ewc_loss(self) -> float:
        """Quadratic penalty pulling parameters toward ``optimal_params`` weighted by Fisher."""
        if not self.fisher:
            return 0.0
        total = 0.0
        params = self.model.named_parameters()
        lam = float(self.lambda_ewc)
        for name, param in params.items():
            if name not in self.fisher:
                continue
            p = np.asarray(param, dtype=np.float64)
            p0 = np.asarray(self.optimal_params[name], dtype=np.float64)
            diff = p - p0
            total += float(np.sum(self.fisher[name] * (diff * diff)))
        return lam * total


class ToyContinualModel:
    """Tiny trainable stub: σ rises with stress plus optional per-anchor drift."""

    def __init__(self, *, dim: int = 8) -> None:
        from .sigma_living_weights import SigmaLivingWeights

        self.living_weights = SigmaLivingWeights(int(dim))
        self._stress = 0.0
        self._anchor_drift: Dict[str, float] = {}
        self._w = np.zeros((dim, dim), dtype=np.float64)

    def surface_sigma(self, prompt: str, answer: str) -> float:
        h = _stable_hash01(f"{prompt}|{answer}")
        base = 0.05 + 0.35 * h
        drift = float(self._anchor_drift.get(prompt, 0.0))
        return float(min(0.99, base + self._stress + drift))

    def save_state(self) -> Dict[str, Any]:
        return {
            "stress": float(self._stress),
            "drift": dict(self._anchor_drift),
            "w": np.array(self._w, copy=True),
            "fw": np.array(self.living_weights.firmware, copy=True),
            "app": np.array(self.living_weights.application, copy=True),
        }

    def load_state(self, state: Any) -> None:
        if not isinstance(state, dict):
            return
        self._stress = float(state.get("stress", 0.0))
        self._anchor_drift = dict(state.get("drift", {}))
        self._w = np.array(state["w"], copy=True)
        self.living_weights.firmware = np.array(state["fw"], copy=True)
        self.living_weights.application = np.array(state["app"], copy=True)

    def train_step(self, new_data: Sequence[Tuple[str, str]], *, lr: float = 0.001) -> None:
        lr_f = float(lr)
        self._stress += lr_f * 0.02
        blob = " ".join(f"{a} {b}" for a, b in new_data)
        # Lab cue: "poison" / adversarial updates inflate σ rapidly (rollback tests).
        if "poison" in blob:
            self._stress += lr_f * 1.2
        for key in list(self._anchor_drift.keys()):
            if key in blob:
                self._anchor_drift[key] = self._anchor_drift.get(key, 0.0) + lr_f * 0.45

    def compute_gradients(self, prompt: str, answer: str) -> Dict[str, np.ndarray]:
        g = (self._w + float(_stable_hash01(prompt))) * 1e-3
        return {"w": g}

    def named_parameters(self) -> Dict[str, np.ndarray]:
        return {"w": self._w}


class ToyContinualGate:
    """Deterministic gate reading surface σ from an attached toy model."""

    def __init__(self, model: ToyContinualModel) -> None:
        self._model = model

    def score(self, prompt: str, answer: str) -> Tuple[float, Verdict]:
        s = float(self._model.surface_sigma(prompt, answer))
        if s < 0.55:
            return s, Verdict.ACCEPT
        if s < 0.85:
            return s, Verdict.RETHINK
        return s, Verdict.ABSTAIN


def _stable_hash01(text: str) -> float:
    import hashlib

    h = hashlib.sha256(str(text).encode("utf-8")).digest()
    return int.from_bytes(h[:4], "big") / float(1 << 32)


__all__ = [
    "ConsolidationTrainable",
    "SigmaConsolidation",
    "SigmaEWC",
    "SigmaGateScorer",
    "ToyContinualGate",
    "ToyContinualModel",
]
