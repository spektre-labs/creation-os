# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
sigma-gate precheck: question-only signal before decoding (HALT / ICR-style motivation).

This module does **not** ship a trained HALT probe. Two modes:

1. ``entropy`` (default): one forward pass on the prompt; use normalized Shannon
   entropy of the **next-token** distribution at the last position as a cheap
   uncertainty proxy. High entropy -> higher ``sigma_pre`` (more uncertain).

2. ``head``: optional pickle bundle ``{"classifier", "scaler", "layer_lo", "layer_hi"}``
   trained offline on concatenated last-token hidden states from layers
   ``[layer_lo, layer_hi)`` (research path; schema is repo-local).

3. ``pooled``: one forward with ``output_hidden_states=True``; mean-pool **prompt**
   tokens per layer for ``layer_range`` (``early`` / ``middle`` / ``late``), concatenate
   layer vectors. If ``head_bundle_path`` loads a dict with ``classifier`` + ``scaler``,
   or a standalone object with ``predict_proba``, use it; else a variance-of-norms
   heuristic (lab default).

See HALT/DRIFT (arXiv:2601.14210) for the scientific claim that intermediate
representations can encode pre-generation risk; this file is an **engineering
scaffold**, not a reproduction of their probe.
"""
from __future__ import annotations

import math
import pickle
from pathlib import Path
from typing import Any, Dict, Optional, Tuple

import numpy as np


def _layer_range_indices(n_layers: int, layer_range: str) -> range:
    """``n_layers`` counts embedding + blocks (HF ``hidden_states`` length)."""
    lr = (layer_range or "middle").strip().lower()
    n = max(1, int(n_layers))
    t = max(1, n // 3)
    if lr == "early":
        return range(0, min(t, n))
    if lr == "late":
        return range(min(2 * t, n - 1), n)
    return range(min(t, n - 1), min(2 * t, n))


class SigmaPrecheck:
    """Return ``(should_generate, sigma_pre)`` from prompt tokens only."""

    def __init__(
        self,
        *,
        tau_skip: float = 0.85,
        mode: str = "entropy",
        head_bundle_path: Optional[str | Path] = None,
        layer_range: str = "middle",
        max_length: int = 256,
    ):
        self.tau_skip = float(tau_skip)
        self.mode = (mode or "entropy").strip().lower()
        self.layer_range = str(layer_range)
        self.max_length = int(max_length)
        self._head: Optional[Dict[str, Any]] = None
        if head_bundle_path is not None:
            p = Path(head_bundle_path).expanduser().resolve()
            if p.is_file():
                self._head = pickle.loads(p.read_bytes())

    def should_generate(self, model: Any, tokenizer: Any, prompt: str) -> Tuple[bool, float]:
        """
        ``should_generate`` is True when ``sigma_pre < tau_skip`` (low enough
        pre-decoding uncertainty to proceed). ``sigma_pre`` is in [0, 1].
        """
        if self.mode == "head" and self._head is not None:
            sigma_pre = self._sigma_from_head(model, tokenizer, prompt)
        elif self.mode == "pooled":
            sigma_pre = self._sigma_from_pooled(model, tokenizer, prompt)
        else:
            sigma_pre = self._sigma_from_entropy(model, tokenizer, prompt)

        return bool(sigma_pre < self.tau_skip), float(sigma_pre)

    def predict(self, model: Any, tokenizer: Any, question: str) -> Tuple[bool, float, Dict[str, Any]]:
        """Same as ``should_generate`` plus a ``details`` dict (HALT-style logging)."""
        should, sigma_pre = self.should_generate(model, tokenizer, question)
        details: Dict[str, Any] = {
            "sigma_pre": float(sigma_pre),
            "should_generate": bool(should),
            "tau_skip": float(self.tau_skip),
            "mode": self.mode,
            "layer_range": self.layer_range,
            "method": "sigma_gate_precheck",
        }
        return should, sigma_pre, details

    def _sigma_from_entropy(self, model: Any, tokenizer: Any, prompt: str) -> float:
        import torch

        inputs = tokenizer(prompt, return_tensors="pt")
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        with torch.no_grad():
            out = model(**inputs)
            logits = out.logits[:, -1, :]
        probs = torch.softmax(logits, dim=-1)
        ent = float((-(probs * (probs + 1e-12).log()).sum(dim=-1)).item())
        vmax = math.log(max(int(probs.shape[-1]), 2))
        return float(max(0.0, min(1.0, ent / vmax)))

    def _sigma_from_head(self, model: Any, tokenizer: Any, prompt: str) -> float:
        import torch

        assert self._head is not None
        clf = self._head["classifier"]
        scaler = self._head["scaler"]
        lo = int(self._head.get("layer_lo", 4))
        hi = int(self._head.get("layer_hi", 9))

        inputs = tokenizer(prompt, return_tensors="pt")
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        with torch.no_grad():
            out = model(**inputs, output_hidden_states=True)
        hs = out.hidden_states
        feats = []
        for l in range(lo, min(hi, len(hs))):
            h = hs[l][:, -1, :].float().cpu().numpy().ravel()
            feats.append(h)
        vec = np.concatenate(feats).reshape(1, -1)
        xs = scaler.transform(vec)
        # positive class = high risk (hallucination) — column index 1 if sklearn binary
        p_risk = float(clf.predict_proba(xs)[0, 1])
        return max(0.0, min(1.0, p_risk))

    def _sigma_from_pooled(self, model: Any, tokenizer: Any, prompt: str) -> float:
        """
        Mean-pool **prompt** token hidden states per layer, concatenate layers, then either
        ``predict_proba`` from a generic pickle object in ``self._head`` (if it is not a
        bundle dict but has ``predict_proba``) or a variance-of-norms heuristic.
        """
        import torch

        inputs = tokenizer(
            prompt,
            return_tensors="pt",
            truncation=True,
            max_length=int(self.max_length),
        )
        dev = next(model.parameters()).device
        inputs = {k: v.to(dev) for k, v in inputs.items()}
        with torch.no_grad():
            out = model(**inputs, output_hidden_states=True)
        hs = out.hidden_states
        layer_idxs = list(_layer_range_indices(len(hs), self.layer_range))
        if not layer_idxs:
            return 0.5
        feats: list[np.ndarray] = []
        for l in layer_idxs:
            t = hs[int(l)][0].float().cpu().numpy()
            feats.append(t.mean(axis=0))
        vec = np.concatenate(feats).reshape(1, -1).astype(np.float64)

        if self._head is not None:
            if isinstance(self._head, dict) and "classifier" in self._head and "scaler" in self._head:
                clf = self._head["classifier"]
                scaler = self._head["scaler"]
                xs = scaler.transform(vec)
                return float(max(0.0, min(1.0, clf.predict_proba(xs)[0, 1])))
            if hasattr(self._head, "predict_proba"):
                try:
                    pr = self._head.predict_proba(vec)
                    if pr.shape[1] >= 2:
                        return float(max(0.0, min(1.0, pr[0, 1])))
                    return float(max(0.0, min(1.0, pr.ravel()[0])))
                except Exception:
                    pass

        norms = [float(np.linalg.norm(f)) for f in feats]
        nv = float(np.var(norms)) if len(norms) > 1 else 0.0
        return float(np.clip(nv / 100.0, 0.0, 1.0))
