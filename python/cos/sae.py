# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Sparse autoencoder V2 (NumPy optional): Top-K codes + σ-gate attribution + steering.

Decompose a fixed-size activation into **approximately sparse** feature coordinates;
:class:`~cos.sigma_gate.SigmaGate` scores prompt–response pairs so we can tag which
features correlate with **high vs low σ** (detect → identify → steer).

This is a **lab scaffold** (random encoder/decoder unless you load weights), not a
trained Anthropic-style SAE on real LM activations. See ``docs/CLAIM_DISCIPLINE.md``.

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
    """Top-K sparse autoencoder with σ-driver analysis, dead-feature stats, and steering."""

    def __init__(
        self,
        input_dim: int = 64,
        hidden_dim: int = 256,
        gate: Optional[Any] = None,
        *,
        seed: int = 42,
        sparsity_k: Optional[int] = None,
        n_features: Optional[int] = None,
    ) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.input_dim = int(input_dim)
        if n_features is not None:
            self.n_features = int(n_features)
        else:
            self.n_features = int(hidden_dim)
        self._sparsity_k_override = int(sparsity_k) if sparsity_k is not None else None
        self._rng_seed = int(seed)

        self.feature_names: Dict[int, str] = {}
        self.feature_sigma: Dict[int, float] = {}
        self.feature_labels: List[str] = [f"feature_{i}" for i in range(self.n_features)]

        if _HAS_NP:
            rng = np.random.default_rng(self._rng_seed)
            # Encoder rows = dictionary atoms: raw = x @ encoder.T + bias  →  (n_features,)
            self.encoder = rng.normal(0.0, 0.01, size=(self.n_features, self.input_dim)).astype(
                np.float32,
                copy=False,
            )
            # decode(sparse) = sparse @ decoder.T  → (input_dim,)  with decoder (input_dim, n_features)
            self.decoder = rng.normal(0.0, 0.01, size=(self.input_dim, self.n_features)).astype(
                np.float32,
                copy=False,
            )
            self.bias = np.zeros(self.n_features, dtype=np.float32)
            self.activation_counts = np.zeros(self.n_features, dtype=np.float64)

    @property
    def hidden_dim(self) -> int:
        """Alias for ``n_features`` (backward compatibility with older constructors)."""
        return int(self.n_features)

    def _effective_k(self) -> int:
        if self._sparsity_k_override is not None:
            return max(1, min(int(self._sparsity_k_override), self.n_features))
        return max(1, self.n_features // 10)

    def encode(
        self,
        activation: Sequence[float],
        *,
        track_counts: bool = True,
    ) -> Dict[str, Any]:
        """Encode a vector into a top-K sparse code (by magnitude of pre-activations)."""
        if not _HAS_NP:
            return {"features": [], "sparsity": 0.0, "n_active": 0}

        x = np.asarray(activation, dtype=np.float32).ravel()
        if x.size < self.input_dim:
            x = np.pad(x, (0, self.input_dim - int(x.size)))
        x = x[: self.input_dim]

        raw = x @ self.encoder.T + self.bias
        k = self._effective_k()
        top_k_idx = np.argsort(np.abs(raw))[-k:]
        sparse = np.zeros(self.n_features, dtype=np.float32)
        sparse[top_k_idx] = raw[top_k_idx]

        if track_counts:
            np.add.at(self.activation_counts, top_k_idx, 1.0)

        active = [(int(i), float(sparse[i])) for i in top_k_idx if float(sparse[i]) != 0.0]
        sparsity = 1.0 - (float(np.count_nonzero(sparse)) / float(self.n_features))

        return {
            "features": active,
            "sparsity": round(float(sparsity), 4),
            "n_active": len(active),
            "sparse": sparse.tolist(),
            "active_features": [int(i) for i in top_k_idx],
        }

    def decode(self, sparse: Sequence[float]) -> Any:
        """Reconstruct an activation vector from a sparse feature code (length ``n_features``)."""
        if not _HAS_NP:
            return None
        z = np.asarray(sparse, dtype=np.float32).ravel()
        if z.size < self.n_features:
            z = np.pad(z, (0, self.n_features - int(z.size)))
        z = z[: self.n_features]
        return z @ self.decoder.T

    def analyze_sigma_drivers(self, test_cases: Sequence[TrainCase]) -> Dict[str, Any]:
        """Count feature ids that fire under high σ (``> 0.5``) vs low σ; summaries + labels."""
        if not _HAS_NP:
            return {"error": "numpy required", "n_features_analyzed": 0}

        feature_high: Dict[int, int] = {}
        feature_low: Dict[int, int] = {}

        for prompt, response in test_cases:
            sigma, _verdict = self.gate.score(str(prompt), str(response))
            text_vec = self._text_to_vector(str(response))
            encoded = self.encode(text_vec, track_counts=True)
            hi = float(sigma) > 0.5
            for feat_id in encoded["active_features"]:
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
            label = self.feature_labels[f_id] if 0 <= f_id < len(self.feature_labels) else f"feature_{f_id}"
            associations.append(
                {
                    "feature_id": f_id,
                    "sigma_association": round(float(sigma_association), 4),
                    "σ_association": round(float(sigma_association), 4),
                    "high_sigma_count": high_c,
                    "low_sigma_count": low_c,
                    "interpretation": interpretation,
                    "label": label,
                }
            )
            self.feature_sigma[f_id] = float(sigma_association)

        associations.sort(key=lambda row: row["sigma_association"], reverse=True)

        hallucination_features = [a for a in associations if a["sigma_association"] > 0.7][:10]
        coherence_features = [a for a in associations if a["sigma_association"] < 0.3][:10]

        return {
            "n_features_analyzed": len(associations),
            "hallucination_features": hallucination_features,
            "coherence_features": coherence_features,
            "all": associations,
            "total_analyzed": len(test_cases),
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
        """Steer by nudging one sparse coordinate then decoding (Golden-Gate-style feature edit)."""
        if not _HAS_NP:
            return list(activation)

        x = np.asarray(activation, dtype=np.float32).ravel()
        if x.size < self.input_dim:
            x = np.pad(x, (0, self.input_dim - int(x.size)))
        x = x[: self.input_dim]

        eff = float(strength)
        if sigma_feature_assoc is not None and eff > 0.0:
            eff *= 1.0 - max(0.0, min(1.0, float(sigma_feature_assoc)))

        enc = self.encode(x, track_counts=False)
        sparse = np.asarray(enc["sparse"], dtype=np.float32).copy()
        fid = int(feature_id)
        if 0 <= fid < self.n_features:
            sparse[fid] = float(sparse[fid]) + eff
        steered = self.decode(sparse)
        if steered is None:
            return x.tolist()
        return steered.astype(np.float32, copy=False).tolist()

    def label_feature(self, feature_id: int, label: str) -> None:
        """Attach a human-readable label to feature ``feature_id``."""
        fid = int(feature_id)
        if fid < 0:
            return
        if fid >= len(self.feature_labels):
            self.feature_labels.extend(
                f"feature_{i}" for i in range(len(self.feature_labels), fid + 1)
            )
        self.feature_labels[fid] = str(label)
        self.feature_names[fid] = str(label)

    def dead_features(self) -> Dict[str, Any]:
        """Features that never received a Top-K hit (capacity utilization lab stat)."""
        if not _HAS_NP:
            return {"dead": [], "n_dead": 0, "utilization": 0.0}
        dead_arr = np.where(self.activation_counts == 0)[0]
        n_dead = int(dead_arr.size)
        util = round(1.0 - n_dead / max(self.n_features, 1), 4)
        return {
            "dead": dead_arr.astype(int).tolist(),
            "n_dead": n_dead,
            "utilization": util,
        }

    def monosemanticity_score(self) -> float:
        """Gini coefficient on non-zero activation counts — toy monosemanticity / peakedness proxy."""
        if not _HAS_NP:
            return 0.5
        active = self.activation_counts[self.activation_counts > 0]
        if active.size == 0:
            return 0.0
        sorted_counts = np.sort(active)
        n = int(sorted_counts.size)
        index = np.arange(1, n + 1, dtype=np.float64)
        gini = (2.0 * float(np.sum(index * sorted_counts)) / (n * float(np.sum(sorted_counts)))) - (
            n + 1
        ) / n
        return round(float(max(0.0, gini)), 4)

    def _text_to_vector(self, text: str) -> List[float]:
        """Deterministic char bag → bounded vector (lab stub; not a neural embedding)."""
        text = str(text)[: self.input_dim]
        chars = [ord(c) % 256 for c in text]
        out = [c / 256.0 for c in chars]
        if len(out) < self.input_dim:
            out.extend([0.0] * (self.input_dim - len(out)))
        return out[: self.input_dim]
