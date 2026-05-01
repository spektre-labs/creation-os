# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
sigma-gate v4: contrastive hidden-state hallucination detector (LSD probe).

Training reference: 5-fold CV AUROC 0.9428 on TruthfulQA-style pairs (see
``benchmarks/sigma_gate_lsd/results_full/manifest.json``). Scoring uses one
forward pass through the probe's frozen causal LM (weights in the pickle
manifest), then trajectory features + logistic head.

The ``model`` and ``tokenizer`` arguments on ``__call__`` / ``compute_sigma`` are
accepted for API compatibility with other gates; they do not replace the
probe encoders unless you retrain the probe for that checkpoint.

Usage::

    export PYTHONPATH=python
    from cos.sigma_gate import SigmaGate
    gate = SigmaGate("benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl")
    sigma, decision = gate(None, None, prompt, response)

Based on LSD (arXiv 2510.04933) contrastive pipeline + Creation OS sigma framing.
"""
from __future__ import annotations

import importlib.util
import sys
import warnings
from pathlib import Path
from typing import Any, Optional, Tuple


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_sigma_gate_lsd_module():
    path = _repo_root() / "benchmarks" / "sigma_probe_lsd" / "sigma_gate_lsd.py"
    name = "_sigma_gate_lsd_cos_runtime"
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError("Cannot load benchmarks/sigma_probe_lsd/sigma_gate_lsd.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


class SigmaGate:
    """Hallucination-oriented sigma in [0, 1] with ACCEPT / RETHINK / ABSTAIN."""

    def __init__(
        self,
        probe_path: str | Path,
        tau_accept: float = 0.3,
        tau_abstain: float = 0.7,
    ):
        self._mod = _load_sigma_gate_lsd_module()
        path = Path(probe_path).expanduser()
        if not path.is_absolute():
            path = (_repo_root() / path).resolve()
        if not path.is_file():
            raise FileNotFoundError(f"Probe bundle not found: {path}")
        self._inner = self._mod.SigmaGateLSD.from_pickle_bundle(path)
        self.tau_accept = float(tau_accept)
        self.tau_abstain = float(tau_abstain)
        self._expected_hf = (self._inner.manifest.get("hf_model") or "").strip()

    def close(self) -> None:
        self._inner.close()

    def __enter__(self) -> SigmaGate:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def _warn_model_mismatch(self, model: Any) -> None:
        if model is None or not self._expected_hf:
            return
        cfg = getattr(model, "config", None)
        name = getattr(cfg, "_name_or_path", None) or getattr(cfg, "name_or_path", None)
        if isinstance(name, str) and name and self._expected_hf not in name and name not in self._expected_hf:
            warnings.warn(
                f"Probe trained for {self._expected_hf!r}; caller model reports {name!r}. "
                "Scores are still computed with the probe checkpoint.",
                stacklevel=3,
            )

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> float:
        """Return sigma = P(hallucination) in [0, 1] (1 - P(factual) from sklearn head)."""
        self._warn_model_mismatch(model)
        del tokenizer
        sigma, _ = self._inner.score(prompt, response, reference=reference)
        return float(sigma)

    def __call__(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> Tuple[float, str]:
        sigma = self.compute_sigma(model, tokenizer, prompt, response, reference=reference)
        if sigma < self.tau_accept:
            return sigma, self._mod.GateString.ACCEPT
        if sigma < self.tau_abstain:
            return sigma, self._mod.GateString.RETHINK
        return sigma, self._mod.GateString.ABSTAIN

    def pack_measurement(self, sigma: float, decision: str, *, tau: Optional[float] = None) -> bytes:
        """12-byte wire blob (see ``SigmaGateLSD.pack_cos_sigma_measurement``)."""
        return self._inner.pack_cos_sigma_measurement(sigma, decision, tau=tau)

    @property
    def struct_size(self) -> int:
        return 12
