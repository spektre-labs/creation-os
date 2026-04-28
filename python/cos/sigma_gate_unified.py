"""
Fused ``σ`` from multiple **uncorrelated engineering signals** (lab path).

Components (defaults):
  * ``σ_probe`` — LSD ``SigmaGate`` pickle (trained trajectory probe).
  * ``σ_spectral`` — attention Laplacian spectrum heuristic (``sigma_spectral``).
  * ``σ_logprob`` — mean normalized next-token entropy over the **tail** of the
    concatenated prompt+response forward pass (cheap uncertainty proxy).

Weights are **not** learned in-tree; pass ``weights=(w_probe, w_spectral, w_logprob)``
and tune offline. This is **not** a published fusion optimum — validate before
external claims.
"""
from __future__ import annotations

import math
from pathlib import Path
from typing import Any, Optional, Tuple

from .sigma_gate import SigmaGate
from .sigma_spectral import spectral_sigma


def logprob_tail_entropy_sigma(
    model: Any,
    tokenizer: Any,
    prompt: str,
    response: str,
    *,
    max_length: int = 512,
    tail_tokens: int = 48,
) -> float:
    """Map mean next-token Shannon entropy (last ``tail`` positions) to [0, 1]."""
    import torch

    text = f"{(prompt or '').strip()}\n{(response or '').strip()}".strip()
    if not text:
        return 0.5
    inp = tokenizer(text, return_tensors="pt", truncation=True, max_length=int(max_length))
    dev = next(model.parameters()).device
    inp = {k: v.to(dev) for k, v in inp.items()}
    with torch.no_grad():
        out = model(**inp)
        logits = out.logits
    probs = torch.softmax(logits[:, :-1], dim=-1)
    ent = -(probs * (probs + 1e-12).log()).sum(dim=-1)
    n = min(int(tail_tokens), int(ent.shape[1]))
    if n <= 0:
        return 0.5
    mean_e = float(ent[0, -n:].mean().item())
    vmax = math.log(max(int(probs.shape[-1]), 2))
    return float(max(0.0, min(1.0, mean_e / vmax)))


class SigmaGateUnified:
    """Weighted fusion of probe + spectral + log-entropy tail signals."""

    def __init__(
        self,
        probe_path: str | Path,
        *,
        weights: Tuple[float, float, float] = (0.6, 0.2, 0.2),
    ):
        self.probe_gate = SigmaGate(probe_path)
        self.w_probe, self.w_spectral, self.w_log = (
            float(weights[0]),
            float(weights[1]),
            float(weights[2]),
        )
        s = self.w_probe + self.w_spectral + self.w_log
        if s <= 0:
            raise ValueError("weights must sum to a positive value")
        self.w_probe /= s
        self.w_spectral /= s
        self.w_log /= s

    def close(self) -> None:
        self.probe_gate.close()

    def __call__(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        response: str,
        *,
        reference: Optional[str] = None,
    ) -> Tuple[float, str]:
        sigma_p, _ = self.probe_gate(model, tokenizer, prompt, response, reference=reference)
        sigma_s, _ = spectral_sigma(model, tokenizer, prompt, response)
        sigma_l = logprob_tail_entropy_sigma(model, tokenizer, prompt, response)
        sigma = self.w_probe * float(sigma_p) + self.w_spectral * float(sigma_s) + self.w_log * float(sigma_l)
        sigma = float(max(0.0, min(1.0, sigma)))
        if sigma < self.probe_gate.tau_accept:
            return sigma, "ACCEPT"
        if sigma < self.probe_gate.tau_abstain:
            return sigma, "RETHINK"
        return sigma, "ABSTAIN"
