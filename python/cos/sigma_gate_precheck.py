"""
sigma-gate precheck: question-only signal before decoding (HALT / ICR-style motivation).

This module does **not** ship a trained HALT probe. Two modes:

1. ``entropy`` (default): one forward pass on the prompt; use normalized Shannon
   entropy of the **next-token** distribution at the last position as a cheap
   uncertainty proxy. High entropy -> higher ``sigma_pre`` (more uncertain).

2. ``head``: optional pickle bundle ``{"classifier", "scaler", "layer_lo", "layer_hi"}``
   trained offline on concatenated last-token hidden states from layers
   ``[layer_lo, layer_hi)`` (research path; schema is repo-local).

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


class SigmaPrecheck:
    """Return ``(should_generate, sigma_pre)`` from prompt tokens only."""

    def __init__(
        self,
        *,
        tau_skip: float = 0.85,
        mode: str = "entropy",
        head_bundle_path: Optional[str | Path] = None,
    ):
        self.tau_skip = float(tau_skip)
        self.mode = mode
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
        else:
            sigma_pre = self._sigma_from_entropy(model, tokenizer, prompt)

        return bool(sigma_pre < self.tau_skip), float(sigma_pre)

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
