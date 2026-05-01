# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
SAE-powered **σ**: sparse autoencoder reconstruction error as a distortion proxy, plus
lightweight **“why”** hooks (active feature indices → optional string labels).

**Orientation:** sparse autoencoders are used in mechanistic interpretability research to
decompose superposed activations; reconstruction error is a **lab prior** for epistemic
strain, not a calibrated production hallucination score without harness JSON.

See ``docs/CLAIM_DISCIPLINE.md`` — do not merge SAE toy numbers with harness AUROC headlines.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Protocol, Sequence, Tuple, runtime_checkable

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[misc, assignment]


@runtime_checkable
class _SAEWithEncodeDecode(Protocol):
    def encode(self, hidden_states: Any) -> Any: ...

    def decode(self, features: Any) -> Any: ...


@runtime_checkable
class _SAECallable(Protocol):
    def __call__(self, hidden_states: Any) -> Any: ...


def _require_torch() -> Any:
    if torch is None:
        raise ImportError("sigma_sae requires PyTorch (`pip install torch`).")
    return torch


class SigmaSAE:
    """
    Wrap a trained SAE (or compatible ``encode``/``decode`` / ``__call__`` module).

    ``compute_sigma`` uses relative L2 reconstruction error in ``[0, 1]`` (clamped).
    """

    def __init__(
        self,
        sae_model: Any,
        *,
        known_hallucination_features: Sequence[int] = (),
        activation_threshold: float = 0.1,
    ) -> None:
        self.sae = sae_model
        self.known_hallucination_features: Tuple[int, ...] = tuple(int(x) for x in known_hallucination_features)
        self.activation_threshold = float(activation_threshold)

    def _flatten_features(self, features: Any) -> Any:
        t = _require_torch()
        if not isinstance(features, t.Tensor):
            raise TypeError("features must be a torch.Tensor.")
        if features.dim() == 1:
            return features
        return features.reshape(-1)

    def _encode_decode(self, hidden_states: Any) -> Tuple[Any, Any]:
        t = _require_torch()
        if not isinstance(hidden_states, t.Tensor):
            raise TypeError("hidden_states must be a torch.Tensor.")

        if isinstance(self.sae, _SAEWithEncodeDecode):
            enc_out = self.sae.encode(hidden_states)
            if isinstance(enc_out, tuple):
                features = enc_out[0]
            else:
                features = enc_out
            reconstructed = self.sae.decode(features)
            return features, reconstructed

        if isinstance(self.sae, _SAECallable):
            out = self.sae(hidden_states)
            if isinstance(out, tuple) and len(out) == 2:
                return out[0], out[1]
            raise TypeError("SAE __call__ must return (features, reconstructed).")

        raise TypeError("sae_model must implement __call__(h)->(z,r) or encode/decode.")

    def encode_decode(self, hidden_states: Any) -> Tuple[Any, Any]:
        """Public wrapper for ``(features, reconstructed)`` (used by superposition / fusion)."""
        return self._encode_decode(hidden_states)

    def compute_sigma(self, hidden_states: Any) -> Tuple[float, Any]:
        """Return ``(sigma_01, features)`` using relative L2 reconstruction error."""
        t = _require_torch()
        h = hidden_states
        features, reconstructed = self.encode_decode(h)
        h32 = h.detach().float()
        r32 = reconstructed.detach().float()
        num = t.linalg.vector_norm(h32 - r32)
        den = t.linalg.vector_norm(h32) + 1e-8
        sigma = float(min(1.0, (num / den).item()))
        return sigma, features

    def decode(self, features: Any) -> Any:
        """Project dictionary features back to activation space (for steering)."""
        if isinstance(self.sae, _SAEWithEncodeDecode):
            return self.sae.decode(features)
        raise TypeError("decode requires an SAE with a decode() method.")

    def explain_sigma(self, features: Any, feature_labels: Sequence[str]) -> List[str]:
        """Return human-readable labels for coordinates above ``activation_threshold``."""
        t = _require_torch()
        z = self._flatten_features(features).detach().float()
        active = t.where(z > self.activation_threshold)[0].tolist()
        out: List[str] = []
        for idx in active:
            if 0 <= int(idx) < len(feature_labels):
                out.append(str(feature_labels[int(idx)]))
        return out

    def detect_hallucination_features(self, features: Any) -> Tuple[bool, List[int]]:
        """
        Return whether any coordinate in ``known_hallucination_features`` fires above
        ``activation_threshold`` (Anthropic-style “feature dictionary” lab pattern).
        """
        _require_torch()
        z = self._flatten_features(features).detach().float()
        active: List[int] = []
        thr = self.activation_threshold
        for i in self.known_hallucination_features:
            if 0 <= i < z.numel() and float(z[i].item()) > thr:
                active.append(int(i))
        return (len(active) > 0), active

    def decompose_dominance_sigma(self, features: Any) -> Tuple[float, List[Tuple[int, float]]]:
        """
        Return ``(dominance, shares)`` where ``dominance`` is ``max_i z_i / sum_j z_j``
        over ReLU features — a **toy** “max feature σ” proxy for superposition strain.
        """
        t = _require_torch()
        z = t.relu(self._flatten_features(features).detach().float())
        s = float(z.sum().item()) + 1e-8
        zmax = float(z.max().item()) if z.numel() else 0.0
        dominance = min(1.0, zmax / s)
        shares: List[Tuple[int, float]] = []
        for i in range(int(z.numel())):
            shares.append((i, float(z[i].item()) / s))
        return dominance, shares

    def aggregate_sigma_sae(self, hidden_states: Any) -> Tuple[float, Any, Dict[str, float]]:
        """
        ``σ_sae ≈ max(reconstruction σ, dominance)`` for cascade L5 lab plumbing.
        """
        recon_sigma, features = self.compute_sigma(hidden_states)
        dom, _ = self.decompose_dominance_sigma(features)
        agg = float(max(float(recon_sigma), float(dom)))
        meta = {"reconstruction_sigma": float(recon_sigma), "dominance_sigma": float(dom)}
        return agg, features, meta

    def clarify_scores(self, features: Any) -> Dict[str, float]:
        """
        Normalized Shannon entropy of positive feature mass — **analogous** to
        “should I clarify?” / spread-of-mass signals; not a trained experiment head.
        """
        t = _require_torch()
        z = t.relu(self._flatten_features(features).detach().float())
        mass = float(z.sum().item())
        if mass <= 1e-12 or z.numel() <= 1:
            return {"clarify_score": 0.0, "output_score": 1.0}
        p = z / (z.sum() + 1e-12)
        ent = float(-(p * p.clamp_min(1e-12).log()).sum().item())
        ent /= math.log(float(z.numel())) + 1e-12
        ent = max(0.0, min(1.0, ent))
        return {"clarify_score": ent, "output_score": 1.0 - ent}

    def cb_sae_feature_metrics(self, features: Any) -> List[Dict[str, Any]]:
        """
        Per-dictionary CB-SAE-shaped audit rows (interpretability / steerability proxies).

        **Lab only:** scores derive from a single activation snapshot, not a probe suite.
        """
        t = _require_torch()
        z = t.relu(self._flatten_features(features).detach().float())
        if z.numel() == 0:
            return []
        zmax = float(z.max().item()) + 1e-12
        med = float(z.median().item())
        std = float(z.std().item()) + 1e-12
        rows: List[Dict[str, Any]] = []
        for i in range(int(z.numel())):
            zi = float(z[i].item())
            interp = max(0.0, min(1.0, zi / zmax))
            steer = max(0.0, min(1.0, abs(zi - med) / std))
            rows.append({"index": i, "interpretability": interp, "steerability": steer})
        return rows

    def plan_cb_sae_prune(
        self,
        features: Any,
        *,
        tau_interp: float = 0.2,
        tau_steer: float = 0.2,
    ) -> Dict[str, Any]:
        """
        Prune coordinates with weak proxy metrics; keep list and name σ-relevant bottleneck tags.
        """
        metrics = self.cb_sae_feature_metrics(features)
        ti, ts = float(tau_interp), float(tau_steer)
        prune: List[int] = []
        keep: List[int] = []
        for row in metrics:
            i = int(row["index"])
            if float(row["interpretability"]) < ti or float(row["steerability"]) < ts:
                prune.append(i)
            else:
                keep.append(i)
        return {
            "prune_indices": prune,
            "keep_indices": keep,
            "augment_concepts": list(SIGMA_CONCEPT_BOTTLENECK),
        }


SIGMA_CONCEPT_BOTTLENECK: Tuple[str, ...] = (
    "factuality",
    "uncertainty",
    "confidence",
    "hedging",
)


__all__ = ["SIGMA_CONCEPT_BOTTLENECK", "SigmaSAE"]
