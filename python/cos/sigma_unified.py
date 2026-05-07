# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
Multi-signal ``σ`` fusion (LSD probe + spectral / sink + response log-prob entropy).

This is a **lab harness** alongside ``SigmaGateUnified`` (probe + legacy ``spectral_sigma``
+ tail entropy). ``SigmaUnified`` wires ``SpectralSigma`` (Laplacian top-k, sink,
EigenTrack-style summaries) with explicit **sink** and **logprob** channels.

Weights are **not** learned in-tree; tune on a validation split before any external
claim that fusion beats the LSD probe alone.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Optional, Tuple

import numpy as np

from .sigma_gate import SigmaGate
from .sigma_spectral import SpectralSigma

from cos.config import DEFAULT_CONFIG


def _norm_weights(w: Dict[str, float]) -> Dict[str, float]:
    s = sum(max(0.0, float(v)) for v in w.values())
    if s <= 0:
        raise ValueError("weights must contain a positive sum")
    return {k: max(0.0, float(v)) / s for k, v in w.items()}


class SigmaUnified:
    """Fuse LSD ``SigmaGate``, ``SpectralSigma`` heuristic, sink mass, and response entropy."""

    def __init__(
        self,
        lsd_probe_path: str | Path,
        *,
        spectral_k: int = 10,
        spectral_laplacian_backend: str = "normalized_symmetric",
        weights: Optional[Dict[str, float]] = None,
        threshold_accept: float | None = None,
        threshold_abstain: float | None = None,
    ):
        ta = float(
            threshold_accept if threshold_accept is not None else DEFAULT_CONFIG.threshold_accept
        )
        tb = float(
            threshold_abstain if threshold_abstain is not None else DEFAULT_CONFIG.threshold_abstain
        )
        self.lsd = SigmaGate(lsd_probe_path, threshold_accept=ta, threshold_abstain=tb)
        self.spectral = SpectralSigma(
            k=int(spectral_k),
            laplacian_backend=str(spectral_laplacian_backend),
        )
        base = weights or {
            "lsd": 0.6,
            "spectral": 0.2,
            "sink": 0.1,
            "logprob": 0.1,
        }
        self.weights = _norm_weights(base)
        self.threshold_accept = float(ta)
        self.threshold_abstain = float(tb)

    def close(self) -> None:
        self.lsd.close()

    @staticmethod
    def compute_logprob_sigma(model: Any, tokenizer: Any, response: str) -> float:
        """Mean token Shannon entropy from a forward pass on ``response`` only, mapped to [0, 1]."""
        import math

        import torch

        text = (response or "").strip()
        if not text:
            return 0.5
        inputs = tokenizer(text, return_tensors="pt", truncation=True, max_length=512)
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        with torch.no_grad():
            out = model(**inputs)
            logits = out.logits
        probs = torch.softmax(logits, dim=-1)
        ent = -(probs * (probs + 1e-12).log()).sum(dim=-1)
        mean_e = float(ent.mean().item())
        vmax = math.log(max(int(probs.shape[-1]), 2))
        return float(np.clip(mean_e / vmax, 0.0, 1.0))

    def __call__(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> Tuple[float, str, Dict[str, Any]]:
        sigma_lsd, _dec_lsd = self.lsd(model, tokenizer, prompt, response, reference=reference)
        sigma_spectral, spectral_features = self.spectral.compute_sigma(model, tokenizer, prompt, response)
        sigma_sink = float(spectral_features.get("mean_sink", 0.0))
        sigma_sink = float(np.clip(sigma_sink, 0.0, 1.0))
        sigma_logprob = self.compute_logprob_sigma(model, tokenizer, response)

        w = self.weights
        sigma = (
            w["lsd"] * float(sigma_lsd)
            + w["spectral"] * float(sigma_spectral)
            + w["sink"] * sigma_sink
            + w["logprob"] * float(sigma_logprob)
        )
        sigma = float(np.clip(sigma, 0.0, 1.0))

        if sigma < self.threshold_accept:
            decision = "ACCEPT"
        elif sigma < self.threshold_abstain:
            decision = "RETHINK"
        else:
            decision = "ABSTAIN"

        details: Dict[str, Any] = {
            "sigma_lsd": float(sigma_lsd),
            "sigma_spectral": float(sigma_spectral),
            "sigma_sink": sigma_sink,
            "sigma_logprob": float(sigma_logprob),
            "sigma_unified": sigma,
            "decision": decision,
            "weights": dict(w),
            "spectral_features": spectral_features,
            "spectral_laplacian_backend": self.spectral.laplacian_backend,
        }
        return sigma, decision, details
