# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-split inference — local-first routing with optional cloud continuation.

**Privacy overrides σ:** ``privacy="high"`` (or no remote endpoint) forces **local** generation.

**Layer split** moves **activations / feature tensors** only — never ships raw prompts from the
local path to the remote path (lab API; wire your transport separately).

Does not modify ``sigma_gate.h``.
"""
from __future__ import annotations

import hashlib
import re
from typing import Any, Dict, Optional, Protocol, Tuple, Union, runtime_checkable

import numpy as np

from .sigma_gate_core import Verdict


def _norm_prompt_key(prompt: str) -> str:
    s = str(prompt).strip().lower()
    s = re.sub(r"\s+", " ", s)
    return s[:2048]


def _prompt_hash(prompt: str) -> str:
    return hashlib.sha256(_norm_prompt_key(prompt).encode("utf-8")).hexdigest()[:24]


@runtime_checkable
class SplitGateLike(Protocol):
    """σ gate + optional semantic cache + feature probe (lab)."""

    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        ...

    def semantic_cache_lookup(self, prompt: str) -> Optional[Dict[str, Any]]:
        ...

    def semantic_cache_store(self, prompt: str, sigma: float, **extra: Any) -> None:
        ...

    def measure_features(self, features: np.ndarray) -> float:
        ...


@runtime_checkable
class LocalInferenceLike(Protocol):
    n_layers: int

    def generate(self, prompt: str) -> str:
        ...

    def forward_partial(self, x: Union[str, np.ndarray], *, layers: range) -> np.ndarray:
        ...


@runtime_checkable
class RemoteInferenceLike(Protocol):
    def generate(self, prompt: str) -> str:
        ...

    def forward_from_features(self, features: np.ndarray, *, start_layer: int) -> np.ndarray:
        ...


class SigmaSplitGate:
    """Small gate helper: σ in [0,1], verdict from thresholds + semantic cache by prompt hash."""

    def __init__(
        self,
        *,
        accept_below: float = 0.35,
        rethink_below: float = 0.72,
    ) -> None:
        self.accept_below = float(accept_below)
        self.rethink_below = float(rethink_below)
        self._semantic: Dict[str, Dict[str, Any]] = {}

    def semantic_cache_lookup(self, prompt: str) -> Optional[Dict[str, Any]]:
        return self._semantic.get(_prompt_hash(prompt))

    def semantic_cache_store(self, prompt: str, sigma: float, **extra: Any) -> None:
        row: Dict[str, Any] = {"sigma": float(sigma)}
        row.update(extra)
        self._semantic[_prompt_hash(prompt)] = row

    def measure_features(self, features: np.ndarray) -> float:
        """Scalar stress on a feature map — L2 normalized (lab)."""
        a = np.asarray(features, dtype=np.float64).ravel()
        if a.size == 0:
            return 0.0
        n = float(np.linalg.norm(a))
        return float(min(1.0, n / (1.0 + n)))

    def score(self, prompt: str, text: str) -> Tuple[float, Verdict]:
        p = str(prompt)
        t = str(text)
        # Heuristic: long heterogeneous completions look harder under local budgets.
        letters = max(1, sum(1 for c in t if c.isalpha()))
        digits = sum(1 for c in t if c.isdigit())
        sigma = 0.08 + 0.35 * min(1.0, len(t) / 800.0) + 0.12 * min(1.0, digits / max(8, letters / 4))
        if t.startswith("REMOTE"):
            sigma *= 0.65
        if "PROOF" in t.upper() or "FERMAT" in t.upper():
            sigma = max(sigma, 0.42)
        sigma = float(min(0.98, sigma))
        self.semantic_cache_store(p, sigma)
        if sigma < self.accept_below:
            return sigma, Verdict.ACCEPT
        if sigma < self.rethink_below:
            return sigma, Verdict.RETHINK
        return sigma, Verdict.ABSTAIN


class SigmaSplitInference:
    """Route whole-model inference between local and remote using σ + privacy policy."""

    def __init__(
        self,
        local_model: LocalInferenceLike,
        *,
        remote_endpoint: Optional[RemoteInferenceLike] = None,
        gate: Optional[SplitGateLike] = None,
        spec_sigma_threshold: float = 0.3,
    ) -> None:
        self.local = local_model
        self.remote = remote_endpoint
        self.gate: SplitGateLike = gate if gate is not None else SigmaSplitGate()
        self.spec_sigma_threshold = float(spec_sigma_threshold)
        self.stats: Dict[str, int] = {"local": 0, "remote": 0, "forced_local": 0}

    def estimate_difficulty(self, prompt: str) -> float:
        """Pre-inference difficulty in ~[0,1] — length + optional cached σ."""
        cached = self.gate.semantic_cache_lookup(prompt)
        if cached and "sigma" in cached:
            return float(cached["sigma"])
        words = max(1, len(str(prompt).split()))
        length_factor = min(words / 100.0, 1.0)
        return float(length_factor * 0.5)

    def infer(self, prompt: str, privacy: str = "normal") -> Dict[str, Any]:
        pr = str(prompt)
        priv = str(privacy).lower().strip()

        if priv == "high" or self.remote is None:
            result = self.local.generate(pr)
            sigma, verdict = self.gate.score(pr, result)
            self.stats["forced_local"] += 1
            return {
                "response": result,
                "sigma": sigma,
                "verdict": verdict.name,
                "location": "local",
                "reason": "privacy" if priv == "high" else "no_remote",
            }

        spec_sigma = self.estimate_difficulty(pr)
        if spec_sigma < self.spec_sigma_threshold:
            result = self.local.generate(pr)
            sigma, verdict = self.gate.score(pr, result)
            if verdict == Verdict.ACCEPT:
                self.stats["local"] += 1
                return {
                    "response": result,
                    "sigma": sigma,
                    "verdict": verdict.name,
                    "location": "local",
                    "reason": "low_spec_sigma",
                }

        result = self.remote.generate(pr)  # type: ignore[union-attr]
        sigma, verdict = self.gate.score(pr, result)
        self.stats["remote"] += 1
        return {
            "response": result,
            "sigma": sigma,
            "verdict": verdict.name,
            "location": "remote",
            "reason": "hard_or_local_uncertain",
        }

    def layer_split(self, prompt: str, *, split_point: int) -> Tuple[np.ndarray, str]:
        """
        Partial forward: layers ``[0, split_point)`` local → feature map; optional remote tail.

        Returns ``(tensor, tag)`` where ``tag`` is ``local_complete`` or ``split``.
        """
        sp = int(split_point)
        if sp <= 0:
            sp = 1
        n = int(self.local.n_layers)
        if sp >= n:
            out = self.local.forward_partial(str(prompt), layers=range(0, n))
            return out, "local_complete"

        local_features = self.local.forward_partial(str(prompt), layers=range(0, sp))
        local_sigma = self.gate.measure_features(local_features)
        if local_sigma < 0.2:
            out = self.local.forward_partial(local_features, layers=range(sp, n))
            return out, "local_complete"
        if self.remote is None:
            out = self.local.forward_partial(local_features, layers=range(sp, n))
            return out, "local_complete_no_remote"

        remote_result = self.remote.forward_from_features(local_features, start_layer=sp)  # type: ignore[union-attr]
        return remote_result, "split"


class ToyLocalModel:
    """Lab local model: weak on long / proof-like prompts."""

    n_layers = 32

    def generate(self, prompt: str) -> str:
        p = str(prompt)
        if len(p) > 400 or "Fermat" in p or "proof" in p.lower():
            return "LOCAL_WEAK: understable sketch only — full proof omitted."
        return f"LOCAL:{p[:120]}"

    def forward_partial(self, x: Union[str, np.ndarray], *, layers: range) -> np.ndarray:
        if isinstance(x, str):
            seed = int(hashlib.sha256(x.encode("utf-8")).hexdigest()[:8], 16)
            v = np.zeros((64,), dtype=np.float64)
            v[0] = float(seed % 997) / 997.0
            v[1] = float(len(x)) / 500.0
            return v
        base = np.asarray(x, dtype=np.float64).ravel()
        if base.size < 64:
            pad = np.zeros(64, dtype=np.float64)
            pad[: base.size] = base
            base = pad
        scale = 1.0 / (1.0 + 0.08 * (layers.stop - layers.start))
        return base * scale


class ToyRemoteModel:
    """Lab remote model: stronger answers (tagged REMOTE)."""

    def generate(self, prompt: str) -> str:
        return f"REMOTE detailed answer for: {str(prompt)[:200]}"

    def forward_from_features(self, features: np.ndarray, *, start_layer: int) -> np.ndarray:
        f = np.asarray(features, dtype=np.float64).ravel()
        tail = np.tanh(f[: min(64, f.size)] + float(start_layer) * 0.01)
        return tail


__all__ = [
    "LocalInferenceLike",
    "RemoteInferenceLike",
    "SigmaSplitGate",
    "SigmaSplitInference",
    "SplitGateLike",
    "ToyLocalModel",
    "ToyRemoteModel",
]
