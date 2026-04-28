# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""
σ-Probe: hidden-state trajectory features for diagnostic σ-gate v3.

Design reference: LSD-style layer trajectories (arXiv 2510.04933) and residual-stream
probing (ICR-style, ACL 2025). This module is **Python / PyTorch** and lives under
``benchmarks/sigma_probe/`` because the repository ``cos`` entrypoint is the built
CLI binary, not a Python package directory.

Not merged into flagship TruthfulQA claims without a repro bundle; lab harness only.
"""

from __future__ import annotations

from typing import Any, Dict, List, Optional

import numpy as np

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore


class SigmaProbe:
    """Hidden-state based σ diagnostic (single forward pass over prompt+response)."""

    def __init__(self, factual_encoder_name: str = "sentence-transformers/all-MiniLM-L6-v2") -> None:
        self.factual_encoder_name = factual_encoder_name
        self._encoder = None
        self.classifier: Any = None
        self.scaler: Any = None
        self.layer_indices: Optional[List[int]] = None

    def _get_encoder(self):
        if self._encoder is None:
            from sentence_transformers import SentenceTransformer

            self._encoder = SentenceTransformer(self.factual_encoder_name)
        return self._encoder

    @staticmethod
    def _cosine_1d(a: np.ndarray, b: np.ndarray) -> float:
        a = np.asarray(a, dtype=np.float64).ravel()
        b = np.asarray(b, dtype=np.float64).ravel()
        m = int(min(a.shape[0], b.shape[0]))
        if m <= 0:
            return 0.0
        a = a[:m]
        b = b[:m]
        na = float(np.linalg.norm(a))
        nb = float(np.linalg.norm(b))
        if na < 1e-12 or nb < 1e-12:
            return 0.0
        return float(np.dot(a, b) / (na * nb))

    def extract_hidden_states(self, model, tokenizer, prompt: str, response: str) -> np.ndarray:
        """Mean-pooled hidden state per layer; shape (n_layers+1, hidden_dim)."""
        if torch is None:
            raise RuntimeError("torch is required for extract_hidden_states")
        text = (prompt or "") + (response or "")
        inputs = tokenizer(
            text,
            return_tensors="pt",
            truncation=True,
            max_length=512,
        )
        device = next(model.parameters()).device
        inputs = {k: v.to(device) for k, v in inputs.items()}
        with torch.no_grad():
            outputs = model(**inputs, output_hidden_states=True)
        hidden_states = outputs.hidden_states
        stacked: List[np.ndarray] = []
        for hs in hidden_states:
            v = hs.float().mean(dim=1).squeeze(0).detach().cpu().numpy()
            stacked.append(np.asarray(v, dtype=np.float64))
        return np.stack(stacked, axis=0)

    def compute_trajectory_features(
        self, layer_embeddings: np.ndarray, factual_embedding: np.ndarray
    ) -> Dict[str, float]:
        """Scalar trajectory features from layer-wise mean embeddings."""
        lem = np.asarray(layer_embeddings, dtype=np.float64)
        fe = np.asarray(factual_embedding, dtype=np.float64).ravel()

        alignments: List[float] = []
        for i in range(lem.shape[0]):
            alignments.append(self._cosine_1d(lem[i], fe))
        alignments_arr = np.asarray(alignments, dtype=np.float64)

        velocities: List[float] = []
        for i in range(1, lem.shape[0]):
            diff = lem[i] - lem[i - 1]
            velocities.append(float(np.linalg.norm(diff)))
        velocities_arr = np.asarray(velocities, dtype=np.float64)

        accelerations = np.diff(velocities_arr) if velocities_arr.size > 1 else np.array([0.0])
        alignment_drift = float(alignments_arr[-1] - alignments_arr[0]) if alignments_arr.size else 0.0
        alignment_std = float(np.std(alignments_arr)) if alignments_arr.size else 0.0

        return {
            "mean_alignment": float(np.mean(alignments_arr)) if alignments_arr.size else 0.0,
            "final_alignment": float(alignments_arr[-1]) if alignments_arr.size else 0.0,
            "alignment_drift": float(alignment_drift),
            "alignment_std": float(alignment_std),
            "mean_velocity": float(np.mean(velocities_arr)) if velocities_arr.size else 0.0,
            "max_velocity": float(np.max(velocities_arr)) if velocities_arr.size else 0.0,
            "velocity_std": float(np.std(velocities_arr)) if velocities_arr.size else 0.0,
            "mean_acceleration": float(np.mean(accelerations)),
            "max_acceleration": float(np.max(np.abs(accelerations))) if accelerations.size else 0.0,
        }

    def compute_sigma(self, model, tokenizer, prompt: str, response: str) -> float:
        """σ ∈ [0, 1]; higher means more uncertain / wrong under trained classifier."""
        enc = self._get_encoder()
        factual_emb = enc.encode(prompt, normalize_embeddings=True)
        if not isinstance(factual_emb, np.ndarray):
            factual_emb = np.asarray(factual_emb, dtype=np.float64)
        factual_emb = factual_emb.ravel()

        layer_embs = self.extract_hidden_states(model, tokenizer, prompt, response)
        features = self.compute_trajectory_features(layer_embs, factual_emb)

        if self.classifier is not None and self.scaler is not None:
            feature_vec = np.array(list(features.values()), dtype=np.float64).reshape(1, -1)
            xs = self.scaler.transform(feature_vec)
            proba = self.classifier.predict_proba(xs)[0]
            # class 1 = wrong / high-σ
            return float(np.clip(proba[1], 0.0, 1.0))

        sigma = float(np.clip(1.0 - features["final_alignment"], 0.0, 1.0))
        return sigma

    def train(self, X: np.ndarray, y: np.ndarray) -> float:
        """Train logistic regression; y=1 wrong (high σ), y=0 correct."""
        from sklearn.linear_model import LogisticRegression
        from sklearn.metrics import roc_auc_score
        from sklearn.preprocessing import StandardScaler

        self.scaler = StandardScaler()
        X_scaled = self.scaler.fit_transform(X)

        self.classifier = LogisticRegression(max_iter=1000, C=1.0, class_weight="balanced")
        self.classifier.fit(X_scaled, y)

        y_prob = self.classifier.predict_proba(X_scaled)[:, 1]
        if len(set(int(t) for t in y)) < 2:
            return float("nan")
        auroc = float(roc_auc_score(y, y_prob))
        print(f"Training AUROC: {auroc:.4f}")
        return auroc

    def save(self, path: str) -> None:
        import pickle as _p

        with open(path, "wb") as f:
            _p.dump(
                {
                    "classifier": self.classifier,
                    "scaler": self.scaler,
                    "factual_encoder_name": self.factual_encoder_name,
                },
                f,
            )

    def load(self, path: str) -> None:
        import pickle as _p

        with open(path, "rb") as f:
            data = _p.load(f)
        self.classifier = data["classifier"]
        self.scaler = data["scaler"]
        self.factual_encoder_name = data.get("factual_encoder_name", self.factual_encoder_name)
