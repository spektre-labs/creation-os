#!/usr/bin/env python3
"""
σ-gate v4: LSD-based trajectory gate (Sirraya contrastive heads + logistic layer).

The frozen checkpoint owns its HuggingFace causal LM and MiniLM truth encoder.
`model` and `tokenizer` arguments on `__call__` / `compute_sigma` are accepted for
API compatibility with other σ-gates but are ignored until a multi-checkpoint path exists.

12-byte wire format: see SigmaGateLSD.pack_cos_sigma_measurement in
benchmarks/sigma_probe_lsd/sigma_gate_lsd.py (cos_sigma_measurement_t shape).
Production import: python/cos/sigma_gate.py with PYTHONPATH=python (README Measured).
"""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from typing import Any, Optional, Tuple


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _load_sigma_gate_lsd_module():
    path = _repo_root() / "benchmarks" / "sigma_probe_lsd" / "sigma_gate_lsd.py"
    name = "_sigma_gate_lsd_runtime"
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise ImportError("Cannot load sigma_gate_lsd.py")
    mod = importlib.util.module_from_spec(spec)
    sys.modules[name] = mod
    spec.loader.exec_module(mod)
    return mod


class SigmaGateV4:
    """Hallucination-oriented σ in [0,1] with ACCEPT / RETHINK / ABSTAIN."""

    def __init__(self, probe_path: str | Path = "benchmarks/sigma_gate_lsd/results/latest/sigma_gate_lsd.pkl"):
        mod = _load_sigma_gate_lsd_module()
        path = Path(probe_path)
        if not path.is_absolute():
            path = (_repo_root() / path).resolve()
        if path.is_file() and path.suffix == ".pkl":
            self._inner = mod.SigmaGateLSD.from_pickle_bundle(path)
        elif path.is_dir():
            self._inner = mod.SigmaGateLSD.from_run_dir(path)
        else:
            raise FileNotFoundError(f"No probe at {path}")
        self.tau_accept = 0.3
        self.tau_rethink = 0.7
        self._mod = mod

    def close(self) -> None:
        self._inner.close()

    def __enter__(self) -> SigmaGateV4:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> float:
        del model, tokenizer
        sigma, _dec = self._inner.score(prompt, response, reference=reference)
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
        if sigma < self.tau_rethink:
            return sigma, self._mod.GateString.RETHINK
        return sigma, self._mod.GateString.ABSTAIN

    def pack_measurement(self, sigma: float, decision: str) -> bytes:
        return self._inner.pack_cos_sigma_measurement(sigma, decision)
