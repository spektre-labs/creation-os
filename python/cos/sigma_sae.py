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

**SplInterp-style caution:** random or trivial baselines can still yield salient dictionary
atoms; treat feature attributions as diagnostic priors, not proven causal structure.

**σ-SAE cascade L5 (v165):** :class:`SigmaSAELabTopK` is a CPU **Top-K sparse** encoder/decoder
lab for ``explain`` / ``steer`` / hallucination correlation — separate from the PyTorch
:class:`SigmaSAE` adapter used by superposition and fusion paths.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
from typing import Any, Dict, Iterable, List, Protocol, Sequence, Tuple, runtime_checkable

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


@runtime_checkable
class _SigmaSAELabGateLike(Protocol):
    """Minimal gate surface for :meth:`SigmaSAELabTopK.find_hallucination_features`."""

    def get_hidden_state(self, prompt: str, response: str) -> Sequence[float]: ...

    def score(self, prompt: str, response: str) -> Tuple[float, str]: ...


def lab_activation_from_text(prompt: str, response: str, dim: int) -> List[float]:
    """Deterministic pseudo-activation in ``[0, 1]`` (no HF model; reproducible lab vectors)."""
    d = max(1, int(dim))
    out: List[float] = []
    seed = hashlib.sha256(f"{prompt}\n{response}".encode("utf-8")).digest()
    i = 0
    while len(out) < d:
        chunk = hashlib.sha256(seed + i.to_bytes(4, "big")).digest()
        for j in range(0, len(chunk), 2):
            if len(out) >= d:
                break
            out.append(int.from_bytes(chunk[j : j + 2], "big") / 65535.0)
        i += 1
    return out


class SigmaSAELabGate:
    """
    TwinGateLab σ scoring plus a reproducible ``get_hidden_state`` for the Top-K diagnostic SAE.
    """

    def __init__(self, input_dim: int) -> None:
        from cos.sigma_twin import TwinGateLab

        self.input_dim = int(input_dim)
        self.lab = TwinGateLab()

    def get_hidden_state(self, prompt: str, response: str) -> List[float]:
        return lab_activation_from_text(prompt, response, self.input_dim)

    def score(self, prompt: str, response: str) -> Tuple[float, str]:
        return self.lab.score(str(prompt), str(response))

    def score_from_activation(self, prompt: str, activation: Sequence[float]) -> Tuple[float, str]:
        """Lab hook: σ on a synthetic response fingerprinted by ``activation`` (for steer A/B)."""
        blob = ",".join(f"{float(x):.8f}" for x in activation)
        syn = "⟨act⟩:" + hashlib.sha256(blob.encode("utf-8")).hexdigest()[:48]
        return self.lab.score(str(prompt), syn)


class SigmaSAELabTopK:
    """
    Top-K sparse autoencoder (lists, no PyTorch) — **σ-SAE L5 diagnostic** / interpretability lab.

    Encoder applies a linear map, keeps the top ``sparsity_k`` positive pre-activations,
    zeros the rest; decoder is linear back to input space.
    """

    def __init__(
        self,
        input_dim: int,
        dict_size: int,
        sparsity_k: int = 32,
        *,
        seed: int = 0,
    ) -> None:
        self.input_dim = int(input_dim)
        self.dict_size = int(dict_size)
        self.k = max(1, int(sparsity_k))
        self._rng_seed = int(seed)

        self.encoder_weights = self._init_matrix(self.input_dim, self.dict_size, salt=1)
        self.encoder_bias = [0.0] * self.dict_size
        self.decoder_weights = self._init_matrix(self.dict_size, self.input_dim, salt=2)
        self.decoder_bias = [0.0] * self.input_dim

        self.feature_labels: Dict[int, str] = {}
        self.feature_stats: Dict[int, Dict[str, Any]] = {
            i: {"activations": 0, "avg_sigma_when_active": 0.0} for i in range(self.dict_size)
        }

    def _init_matrix(self, rows: int, cols: int, *, salt: int) -> List[List[float]]:
        scale = 0.07
        w: List[List[float]] = []
        for i in range(rows):
            row: List[float] = []
            for j in range(cols):
                z = math.sin((i * 12.9898 + j * 78.233 + salt * 3.14 + self._rng_seed) * 0.001)
                row.append(scale * z)
            w.append(row)
        return w

    def encode(self, activation: Sequence[float]) -> Tuple[List[float], List[int]]:
        """Linear encoder + Top-K positive sparsity → ``(sparse_vector, active_indices)``."""
        if len(activation) != self.input_dim:
            raise ValueError(f"activation dim {len(activation)} != input_dim {self.input_dim}")
        act = [float(x) for x in activation]
        pre_act: List[float] = []
        for i in range(self.dict_size):
            s = float(self.encoder_bias[i]) + sum(
                act[j] * self.encoder_weights[j][i] for j in range(self.input_dim)
            )
            pre_act.append(s)
        indexed = [(val, idx) for idx, val in enumerate(pre_act)]
        indexed.sort(reverse=True, key=lambda t: t[0])
        sparse = [0.0] * self.dict_size
        active_features: List[int] = []
        for val, idx in indexed[: self.k]:
            if val > 0.0:
                sparse[idx] = val
                active_features.append(int(idx))
        return sparse, active_features

    def decode(self, sparse: Sequence[float]) -> List[float]:
        if len(sparse) != self.dict_size:
            raise ValueError(f"sparse len {len(sparse)} != dict_size {self.dict_size}")
        sp = [float(x) for x in sparse]
        return [
            float(self.decoder_bias[i])
            + sum(sp[j] * self.decoder_weights[j][i] for j in range(self.dict_size))
            for i in range(self.input_dim)
        ]

    def reconstruction_sigma(self, activation: Sequence[float]) -> float:
        """Relative L2 reconstruction error in ``[0, 1]`` (aligned with the torch adapter intuition)."""
        act = [float(x) for x in activation]
        sparse, _ = self.encode(act)
        recon = self.decode(sparse)
        num = math.sqrt(sum((a - r) ** 2 for a, r in zip(act, recon)))
        den = math.sqrt(sum(a * a for a in act)) + 1e-8
        return float(min(1.0, num / den))

    def explain_sigma(self, activation: Sequence[float], sigma: float) -> Dict[str, Any]:
        """
        Attribute a scalar σ (from the gate) to active dictionary features.

        ``likely_cause`` uses running ``avg_sigma_when_active`` (from ``find_hallucination_features`` /
        online updates); without prior stats, defaults hover near zero and the flag is conservative.
        """
        sparse, active = self.encode([float(x) for x in activation])
        ranked = sorted(active, key=lambda i: float(sparse[int(i)]), reverse=True)
        head = {int(x) for x in ranked[: min(3, len(ranked))]}
        explanations: List[Dict[str, Any]] = []
        alpha = 0.05
        for feat_id in active:
            stats = self.feature_stats[int(feat_id)]
            stats["activations"] = int(stats["activations"]) + 1
            stats["avg_sigma_when_active"] = float(
                alpha * float(sigma) + (1.0 - alpha) * float(stats["avg_sigma_when_active"])
            )
            label = self.feature_labels.get(int(feat_id), f"feature_{feat_id}")
            avg_s = float(stats["avg_sigma_when_active"])
            explanations.append(
                {
                    "feature": int(feat_id),
                    "label": label,
                    "activation_strength": float(sparse[feat_id]),
                    "avg_sigma_when_active": avg_s,
                    "likely_cause": bool(avg_s > 0.5) or (float(sigma) > 0.55 and int(feat_id) in head),
                }
            )
        explanations.sort(key=lambda x: x["avg_sigma_when_active"], reverse=True)
        return {
            "sigma": float(sigma),
            "active_features": len(active),
            "likely_causes": [e for e in explanations if e["likely_cause"]],
            "all_features": explanations[:10],
            "cascade_level": "L5_SAE",
        }

    def steer(self, activation: Sequence[float], feature_id: int, strength: float = 0.0) -> List[float]:
        """Override Top-K coordinate ``feature_id`` then decode (lab steering primitive)."""
        sparse, _ = self.encode([float(x) for x in activation])
        fid = int(feature_id)
        if not (0 <= fid < self.dict_size):
            raise IndexError(f"feature_id {fid} out of range [0,{self.dict_size})")
        sparse[fid] = float(strength)
        return self.decode(sparse)

    def find_hallucination_features(
        self,
        dataset: Iterable[Any],
        gate: _SigmaSAELabGateLike,
        *,
        min_support: int = 11,
    ) -> List[Dict[str, Any]]:
        """
        Correlate dictionary features with labelled hallucination rows.

        ``dataset`` yields either ``(prompt, response, is_hallucination)`` tuples or dicts with
        keys ``prompt`` / ``response`` / ``is_hallucination`` (or ``hallucination`` bool).

        Features with fewer than ``min_support`` firings are omitted (default 11: SplInterp-safe
        small-sample guard; lower in unit tests).
        """
        hallucination_features: Dict[int, Dict[str, int]] = {}
        alpha = 0.01

        for row in dataset:
            prompt, response, is_hall = _sae_lab_coerce_row(row)
            act = [float(x) for x in gate.get_hidden_state(prompt, response)]
            sigma, _v = gate.score(prompt, response)
            sparse, active = self.encode(act)
            for feat_id in active:
                fid = int(feat_id)
                if fid not in hallucination_features:
                    hallucination_features[fid] = {"total": 0, "hallucination": 0}
                hallucination_features[fid]["total"] += 1
                if is_hall:
                    hallucination_features[fid]["hallucination"] += 1
                stats = self.feature_stats[fid]
                stats["activations"] = int(stats["activations"]) + 1
                stats["avg_sigma_when_active"] = float(
                    alpha * float(sigma) + (1.0 - alpha) * float(stats["avg_sigma_when_active"])
                )

        results: List[Dict[str, Any]] = []
        for fid, st in hallucination_features.items():
            total = int(st["total"])
            if total <= int(min_support):
                continue
            rate = float(st["hallucination"]) / float(total)
            results.append(
                {
                    "feature": int(fid),
                    "hallucination_rate": rate,
                    "total_activations": total,
                    "is_hallucination_feature": bool(rate > 0.7),
                }
            )
        results.sort(key=lambda x: x["hallucination_rate"], reverse=True)
        return results


def _sae_lab_coerce_row(row: Any) -> Tuple[str, str, bool]:
    if isinstance(row, (list, tuple)) and len(row) >= 3:
        return str(row[0]), str(row[1]), bool(row[2])
    if isinstance(row, dict):
        pr = str(row.get("prompt", ""))
        rs = str(row.get("response", ""))
        h = row.get("is_hallucination")
        if h is None:
            h = row.get("hallucination", False)
        return pr, rs, bool(h)
    raise TypeError("dataset row must be tuple/list of length ≥3 or a dict with prompt/response flags")


def sigma_sae_lab_load_dataset_json(path: str) -> List[Any]:
    """Load a JSON list of rows for ``find_hallucination_features``."""
    p = Path(path).expanduser()
    raw = json.loads(p.read_text(encoding="utf-8"))
    if not isinstance(raw, list):
        raise ValueError("dataset JSON must be a list")
    return raw


SIGMA_CONCEPT_BOTTLENECK: Tuple[str, ...] = (
    "factuality",
    "uncertainty",
    "confidence",
    "hedging",
)


__all__ = [
    "SIGMA_CONCEPT_BOTTLENECK",
    "SigmaSAE",
    "SigmaSAELabGate",
    "SigmaSAELabTopK",
    "lab_activation_from_text",
    "sigma_sae_lab_load_dataset_json",
]
