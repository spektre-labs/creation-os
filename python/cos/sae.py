# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Lab sparse autoencoder-style feature view over σ-gate-facing vectors (NumPy optional).

Top-K ReLU codes + random encoder/decoder (``seed=42``) illustrate **σ attribution**: which
latent ids co-occur with high vs low σ on string pairs. This is **not** a trained Anthropic-style
SAE on LLM activations; it is an interpretability **scaffold** aligned with ``SigmaGate``.

Feature steering accepts an optional ``sigma_feature_assoc`` in ``[0, 1]`` (fraction of times the
latent fired under high σ in ``analyze_sigma_drivers``): higher association **dampens** positive
amplification to avoid pushing directions linked to unstable / high-σ regimes (lab policy hook).

**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple, Union

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaSAE"]

try:  # pragma: no cover - import guard exercised when NumPy missing
    import numpy as np

    _HAS_NP = True
except ImportError:  # pragma: no cover
    np = None  # type: ignore[assignment]
    _HAS_NP = False

TrainCase = Tuple[str, str]


class SigmaSAE:
    """Toy sparse encoder for σ attribution and steering (random weights unless you replace them)."""

    def __init__(
        self,
        input_dim: int = 64,
        hidden_dim: int = 256,
        gate: Optional[Any] = None,
        *,
        seed: int = 42,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.input_dim = int(input_dim)
        self.hidden_dim = int(hidden_dim)
        self.feature_names: Dict[int, str] = {}
        self.feature_sigma: Dict[int, float] = {}
        self._rng_seed = int(seed)

        if _HAS_NP:
            rng = np.random.default_rng(self._rng_seed)
            self.encoder = rng.normal(0.0, 0.01, size=(self.input_dim, self.hidden_dim)).astype(
                np.float32,
                copy=False,
            )
            self.decoder = rng.normal(0.0, 0.01, size=(self.hidden_dim, self.input_dim)).astype(
                np.float32,
                copy=False,
            )
            self.bias = np.zeros(self.hidden_dim, dtype=np.float32)

    def encode(self, activation: Sequence[float]) -> Dict[str, Any]:
        """Encode a vector into a top-K sparse ReLU code."""
        if not _HAS_NP:
            return {"features": [], "sparsity": 0.0, "n_active": 0}

        x = np.asarray(activation, dtype=np.float32).ravel()
        if x.size < self.input_dim:
            x = np.pad(x, (0, self.input_dim - int(x.size)))
        x = x[: self.input_dim]

        hidden_raw = x @ self.encoder + self.bias
        hidden = np.maximum(0.0, hidden_raw)

        k = max(1, self.hidden_dim // 10)
        top_k_idx = np.argsort(hidden)[-k:]
        sparse = np.zeros_like(hidden)
        sparse[top_k_idx] = hidden[top_k_idx]

        active = [(int(i), float(sparse[i])) for i in top_k_idx if float(sparse[i]) > 0.0]
        sparsity = 1.0 - (float(np.count_nonzero(sparse)) / float(len(sparse)))

        return {
            "features": active,
            "sparsity": round(float(sparsity), 4),
            "n_active": len(active),
        }

    def analyze_sigma_drivers(self, test_cases: Sequence[TrainCase]) -> Dict[str, Any]:
        """Count latent ids that fire under high σ (``> 0.5``) vs low σ; fill ``feature_sigma`` summaries."""
        if not _HAS_NP:
            return {"error": "numpy required", "n_features_analyzed": 0}

        feature_high: Dict[int, int] = {}
        feature_low: Dict[int, int] = {}

        for prompt, response in test_cases:
            sigma, _verdict = self.gate.score(str(prompt), str(response))
            text_vec = self._text_to_vector(str(response))
            encoded = self.encode(text_vec)
            hi = float(sigma) > 0.5
            for feat_id, _act in encoded["features"]:
                if hi:
                    feature_high[feat_id] = feature_high.get(feat_id, 0) + 1
                else:
                    feature_low[feat_id] = feature_low.get(feat_id, 0) + 1

        all_features = set(feature_high.keys()) | set(feature_low.keys())
        associations: List[Dict[str, Any]] = []
        for f_id in all_features:
            high_c = int(feature_high.get(f_id, 0))
            low_c = int(feature_low.get(f_id, 0))
            total = high_c + low_c
            if total <= 0:
                continue
            sigma_association = high_c / float(total)
            if sigma_association > 0.7:
                interpretation = "high_sigma_driver"
            elif sigma_association < 0.3:
                interpretation = "low_sigma_driver"
            else:
                interpretation = "neutral"
            associations.append(
                {
                    "feature_id": f_id,
                    "sigma_association": round(float(sigma_association), 4),
                    "high_sigma_count": high_c,
                    "low_sigma_count": low_c,
                    "interpretation": interpretation,
                }
            )
            self.feature_sigma[f_id] = float(sigma_association)

        associations.sort(key=lambda row: row["sigma_association"], reverse=True)
        return {
            "n_features_analyzed": len(associations),
            "hallucination_features": [a for a in associations if a["sigma_association"] > 0.7],
            "coherence_features": [a for a in associations if a["sigma_association"] < 0.3],
            "all": associations,
        }

    def analyze_σ_drivers(self, test_cases: Sequence[TrainCase]) -> Dict[str, Any]:
        """Alias for :meth:`analyze_sigma_drivers` (σ-attribution vocabulary)."""
        return self.analyze_sigma_drivers(test_cases)

    def steer(
        self,
        activation: Sequence[float],
        feature_id: int,
        strength: float = 1.0,
        *,
        sigma_feature_assoc: Optional[float] = None,
    ) -> Union[List[float], Sequence[float]]:
        """Move ``activation`` along decoder row ``feature_id``; dampen positive strength when assoc is high."""
        if not _HAS_NP:
            return list(activation)

        x = np.asarray(activation, dtype=np.float32).ravel()
        if x.size < self.input_dim:
            x = np.pad(x, (0, self.input_dim - int(x.size)))
        x = x[: self.input_dim]

        eff = float(strength)
        if sigma_feature_assoc is not None and eff > 0.0:
            eff *= 1.0 - max(0.0, min(1.0, float(sigma_feature_assoc)))

        direction = self.decoder[int(feature_id)]
        steered = x + eff * direction
        return steered.tolist()

    def _text_to_vector(self, text: str) -> List[float]:
        """Deterministic char bag → bounded vector (lab stub; not a neural embedding)."""
        text = str(text)[: self.input_dim]
        chars = [ord(c) % 256 for c in text]
        out = [c / 256.0 for c in chars]
        if len(out) < self.input_dim:
            out.extend([0.0] * (self.input_dim - len(out)))
        return out[: self.input_dim]
