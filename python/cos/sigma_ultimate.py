# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
``SigmaUltimate``: fuse **LSD** (optional pickle), **HIDE** (``SigmaHIDE``), and
``SpectralSigma`` into one **cognitive interrupt** verdict via ``sigma_gate_core``
(mirror of ``sigma_gate.h``).

This is a **lab harness** — not a published fusion optimum. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from pathlib import Path
from typing import Any, Dict, Optional, Tuple

import numpy as np

from .sigma_gate import SigmaGate
from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update
from .sigma_hide import SigmaHIDE
from .sigma_spectral import SpectralSigma


def _norm_weights(w: Dict[str, float]) -> Dict[str, float]:
    s = sum(max(0.0, float(v)) for v in w.values())
    if s <= 0:
        raise ValueError("weights must contain a positive sum")
    return {k: max(0.0, float(v)) / s for k, v in w.items()}


class SigmaUltimate:
    def __init__(
        self,
        lsd_probe_path: Optional[str | Path] = None,
        *,
        spectral_k: int = 10,
        spectral_laplacian_backend: str = "normalized_symmetric",
        hide_backend: str = "auto",
        weights: Optional[Dict[str, float]] = None,
        tau_accept: float = 0.3,
        tau_abstain: float = 0.7,
        vote_tau: float = 0.6,
    ):
        self._legacy_tau = (float(tau_accept), float(tau_abstain))
        self.lsd: Optional[SigmaGate] = None
        if lsd_probe_path is not None:
            self.lsd = SigmaGate(lsd_probe_path)
        self.hide = SigmaHIDE(backend=str(hide_backend))
        self.spectral = SpectralSigma(
            k=int(spectral_k),
            laplacian_backend=str(spectral_laplacian_backend),
        )
        if weights is None:
            if self.lsd is not None:
                weights = {"lsd": 0.5, "hide": 0.3, "spectral": 0.2}
            else:
                weights = {"hide": 0.55, "spectral": 0.45}
        self.weights = _norm_weights(weights)
        self.vote_tau = float(vote_tau)

    def close(self) -> None:
        if self.lsd is not None:
            self.lsd.close()

    def __call__(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
        max_new_tokens: int = 100,
    ) -> Tuple[float, str, Dict[str, Any]]:
        signals: Dict[str, float] = {}

        if self.lsd is not None:
            s = self.lsd.compute_sigma(model, tokenizer, prompt, response, reference=reference)
            signals["lsd"] = float(s)
        else:
            signals["lsd"] = 0.0

        s_h, _det = self.hide.compute_sigma(
            model, tokenizer, prompt, response, max_new_tokens=int(max_new_tokens)
        )
        signals["hide"] = float(s_h)

        s_sp, _f = self.spectral.compute_sigma(model, tokenizer, prompt, response)
        signals["spectral"] = float(s_sp)

        combined = sum(
            self.weights.get(k, 0.0) * float(signals.get(k, 0.0)) for k in ("lsd", "hide", "spectral")
        )
        combined = float(np.clip(combined, 0.0, 1.0))

        high = sum(1 for k in ("lsd", "hide", "spectral") if float(signals.get(k, 0.0)) > self.vote_tau)
        if high >= 2:
            combined = float(min(1.0, combined + 0.15))

        st = SigmaState()
        sigma_update(st, combined, max(0.0, min(1.0, 1.0 - combined)))
        verdict = sigma_gate(st)
        decision = verdict.name

        return combined, decision, {
            "signals": dict(signals),
            "weights": dict(self.weights),
            "voting_high": int(high),
            "sigma_final": combined,
            "decision": decision,
            "cognitive_verdict": decision,
            "k_eff_q16": st.k_eff,
            "d_sigma_q16": st.d_sigma,
            "legacy_tau_unused": self._legacy_tau,
        }
