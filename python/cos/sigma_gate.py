# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
σ-gate: default **zero-dependency** entropy scorer, or full **LSD probe** when a bundle path
is passed.

**Lite mode** (``SigmaGate()``): portable ``pip install creation-os`` — no PyTorch.

**LSD mode** (``SigmaGate("path/to/sigma_gate_lsd.pkl")``): contrastive hidden-state
hallucination detector; requires ``creation-os[probes]`` and probe assets (see
``benchmarks/sigma_gate_lsd/`` in a full tree checkout).

Usage::

    from cos.sigma_gate import SigmaGate
    sigma, verdict = SigmaGate().score("What is 2+2?", "4")

    gate = SigmaGate("benchmarks/sigma_gate_lsd/results_full/sigma_gate_lsd.pkl")
    sigma, decision = gate(None, None, prompt, response)
"""
from __future__ import annotations

import importlib.util
import math
import os
import sys
import warnings
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

from cos.config import DEFAULT_CONFIG

ACCEPT = "ACCEPT"
RETHINK = "RETHINK"
ABSTAIN = "ABSTAIN"


def _entropy_signal(blob: str) -> float:
    """Character-level entropy on ``blob`` → sigma proxy (lower σ = calmer / more repetitive)."""
    if not blob:
        return 1.0
    freq: dict[str, int] = {}
    for c in blob.lower():
        freq[c] = freq.get(c, 0) + 1
    total = len(blob)
    entropy = -sum(
        (count / total) * math.log2(count / total) for count in freq.values() if count > 0
    )
    normalized = min(1.0, entropy / 4.7)
    sigma = 1.0 - normalized
    return max(0.0, min(1.0, sigma))


def _lite_entropy_pair(prompt: str, response: str) -> float:
    """Include **prompt** so terse factual answers (``4``) inherit context entropy."""
    if not str(response).strip():
        return 1.0
    blob = f"{str(prompt).strip()}\n{str(response).strip()}"
    return float(_entropy_signal(blob))


def _repo_root() -> Path:
    env = (os.environ.get("CREATION_OS_ROOT") or "").strip()
    if env:
        return Path(env).expanduser().resolve()
    here = Path(__file__).resolve()
    for base in (Path.cwd().resolve(), *here.parents):
        if (base / "creation_os_v2.c").is_file():
            return base
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

    ACCEPT = ACCEPT
    RETHINK = RETHINK
    ABSTAIN = ABSTAIN

    def __init__(
        self,
        probe_path: str | Path | None = None,
        *,
        threshold_accept: Optional[float] = None,
        threshold_abstain: Optional[float] = None,
    ) -> None:
        _bands = type(DEFAULT_CONFIG)(
            threshold_accept=float(threshold_accept if threshold_accept is not None else DEFAULT_CONFIG.threshold_accept),
            threshold_abstain=float(threshold_abstain if threshold_abstain is not None else DEFAULT_CONFIG.threshold_abstain),
        )
        ta = float(_bands.threshold_accept)
        tb = float(_bands.threshold_abstain)
        self._mode: str
        self._ema: float
        self._count: int
        self._inner: Any
        self._mod: Any
        self._expected_hf: str

        path = probe_path
        empty = path is None or (isinstance(path, str) and not str(path).strip())

        if empty:
            self._mode = "lite"
            self.threshold_accept = ta
            self.threshold_abstain = tb
            self._ema = 0.5
            self._count = 0
            self._inner = None
            self._mod = None
            self._expected_hf = ""
            return

        self._mode = "lsd"
        self._mod = _load_sigma_gate_lsd_module()
        p = Path(path).expanduser()
        if not p.is_absolute():
            p = (_repo_root() / p).resolve()
        if not p.is_file():
            raise FileNotFoundError(f"Probe bundle not found: {p}")
        self._inner = self._mod.SigmaGateLSD.from_pickle_bundle(p)
        self.threshold_accept = ta
        self.threshold_abstain = tb
        self._expected_hf = (self._inner.manifest.get("hf_model") or "").strip()
        self._ema = 0.5
        self._count = 0

    @property
    def threshold_accept(self) -> float:
        """σ below this → ACCEPT (lite / LSD)."""
        return self._threshold_accept

    @threshold_accept.setter
    def threshold_accept(self, v: float) -> None:
        self._threshold_accept = float(v)

    @property
    def threshold_abstain(self) -> float:
        """σ at or above this → ABSTAIN."""
        return self._threshold_abstain

    @threshold_abstain.setter
    def threshold_abstain(self, v: float) -> None:
        self._threshold_abstain = float(v)

    def close(self) -> None:
        if self._mode == "lsd" and self._inner is not None:
            self._inner.close()

    def __enter__(self) -> SigmaGate:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    @property
    def avg_sigma(self) -> float:
        return float(self._ema)

    def _warn_model_mismatch(self, model: Any) -> None:
        if self._mode != "lsd" or model is None or not self._expected_hf:
            return
        cfg = getattr(model, "config", None)
        name = getattr(cfg, "_name_or_path", None) or getattr(cfg, "name_or_path", None)
        if isinstance(name, str) and name and self._expected_hf not in name and name not in self._expected_hf:
            warnings.warn(
                f"Probe trained for {self._expected_hf!r}; caller model reports {name!r}. "
                "Scores are still computed with the probe checkpoint.",
                stacklevel=3,
            )

    def _verdict(self, sigma: float) -> str:
        if sigma < self.threshold_accept:
            return ACCEPT
        if sigma < self.threshold_abstain:
            return RETHINK
        return ABSTAIN

    def _lite_update_ema(self, sigma: float) -> None:
        self._ema = 0.9 * self._ema + 0.1 * sigma
        self._count += 1

    def compute_sigma(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> float:
        """Return sigma in ``[0, 1]`` (lite: entropy on ``response``; LSD: probe head)."""
        if self._mode == "lite":
            del model, tokenizer, reference
            return float(_lite_entropy_pair(prompt, response))

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
        if self._mode == "lite":
            self._lite_update_ema(sigma)
            return float(sigma), self._verdict(sigma)
        if sigma < self.threshold_accept:
            return sigma, self._mod.GateString.ACCEPT
        if sigma < self.threshold_abstain:
            return sigma, self._mod.GateString.RETHINK
        return sigma, self._mod.GateString.ABSTAIN

    def score(
        self,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> Tuple[float, str]:
        """Score a pair; LSD mode uses the frozen probe only (no external HF model)."""
        return self(None, None, prompt, response, reference=reference)

    def score_cascade(
        self,
        prompt: str,
        response: str,
        hidden_states: Any = None,
        attention_maps: Any = None,
    ) -> Dict[str, Any]:
        """
        Multi-level cascade dict. **L1** always runs. **L2–L5** use ``hidden_states``.
        **L6** (sink + spectral) uses ``attention_maps`` when provided. Requires ``cos.cascade``.
        """
        levels: Dict[str, Any] = {}
        levels["L1_entropy"] = float(_lite_entropy_pair(prompt, response))
        sigma = float(levels["L1_entropy"])

        l2_l5_ok = False
        if hidden_states is not None:
            try:
                from cos.cascade import (  # type: ignore[import-not-found]
                    cascade_L2,
                    cascade_L3,
                    cascade_L4,
                    cascade_L5,
                )

                levels["L2_hide"] = float(cascade_L2(hidden_states))
                levels["L3_icr"] = float(cascade_L3(hidden_states))
                levels["L4_lsd"] = float(cascade_L4(hidden_states))
                levels["L5_sae"] = float(cascade_L5(hidden_states))
                l2_l5_ok = True
            except ImportError:
                levels["cascade_import"] = "L2_L5_unavailable_optional_cos_cascade"

        if attention_maps is not None:
            try:
                from cos.cascade import cascade_L6  # type: ignore[import-not-found]

                levels["L6_sink"] = float(cascade_L6(attention_maps, hidden_states))
            except ImportError:
                levels["cascade_import_L6"] = "L6_unavailable_optional_cos_cascade"

        if not (sigma < 0.1 or sigma > 0.9):
            has_l6 = "L6_sink" in levels
            if l2_l5_ok and has_l6:
                sigma = (
                    0.24 * levels["L1_entropy"]
                    + 0.22 * levels["L2_hide"]
                    + 0.17 * levels["L3_icr"]
                    + 0.12 * levels["L4_lsd"]
                    + 0.06 * levels["L5_sae"]
                    + 0.19 * levels["L6_sink"]
                )
            elif l2_l5_ok:
                sigma = (
                    0.3 * levels["L1_entropy"]
                    + 0.3 * levels["L2_hide"]
                    + 0.2 * levels["L3_icr"]
                    + 0.15 * levels["L4_lsd"]
                    + 0.05 * levels["L5_sae"]
                )
            elif has_l6:
                sigma = 0.65 * levels["L1_entropy"] + 0.35 * levels["L6_sink"]

        if self._mode == "lsd" and hidden_states is None:
            ps, _ = self._inner.score(prompt, response, reference=None)
            sigma = float(ps)
            levels["LSD_probe"] = float(ps)

        if self._mode == "lite":
            self._lite_update_ema(float(sigma))

        verdict = self._verdict(float(sigma))
        return {"sigma": float(sigma), "verdict": verdict, "levels": levels}

    def pack_measurement(self, sigma: float, decision: str, *, tau: Optional[float] = None) -> bytes:
        """12-byte wire blob (LSD probe only)."""
        if self._mode != "lsd" or self._inner is None:
            raise NotImplementedError("pack_measurement requires SigmaGate(probe_path=...) LSD bundle")
        return self._inner.pack_cos_sigma_measurement(sigma, decision, tau=tau)

    @property
    def struct_size(self) -> int:
        return 12
