# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Streaming / per-token **σ** scaffold and **halt-capable** decode loop.

**Lab harness only.** Motivated by token-level probe work (e.g. entity-oriented checks;
arXiv:2509.03531 as **orientation only**). Heuristic path uses **normalized predictive
entropy** ``H / log(V)`` in ``[0,1]`` (``V`` = vocab size) so ``tau_halt`` is comparable
across LMs. Halting requires **``min_consecutive_high``** scores ``> tau_halt`` inside the
last ``window_size`` generation scores (not a single spurious spike). See
``docs/CLAIM_DISCIPLINE.md``.

``StreamingSigmaScaffold`` records a cheap scalar from one decoder block (L2 norm of the
last-position hidden, squashed to ``[0,1]``).

``StreamingSigmaGate`` supports an optional pickle probe (``predict_proba`` on last hidden)
or the **normalized entropy** heuristic above.

**Heuristic path (no pickle probe):** halting uses a **sliding window** on
``σ = H / log(V)`` (see ``normalized_entropy_from_logits``): after at least
``min_gen_tokens_before_halt`` generated tokens, if any ``min_consecutive_high`` consecutive
scores in the trailing ``window_size`` frame are all **strictly** ``> tau_halt``, generation
stops. Defaults ``tau_halt=0.95``, ``window_size=8`` — tune locally; see
``docs/CLAIM_DISCIPLINE.md`` before publishing halt-rate headlines.

**Pickle-probe path:** each scored hidden still feeds ``sigma_gate_core`` (mirror of
``sigma_gate.h``): **ABSTAIN** halts; **RETHINK** / **ACCEPT** continue.
"""
from __future__ import annotations

import pickle
from pathlib import Path
from typing import Any, Callable, List, Optional

import numpy as np

from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


def normalized_entropy_from_logits(logits_1v: Any) -> float:
    """
    Shannon entropy of next-token distribution, divided by ``log(vocab)`` → ``[0,1]``.

    High value ≈ flat / uncertain next-token mass (used as **risk** for halt policy).
    """
    import torch
    import torch.nn.functional as F

    if not isinstance(logits_1v, torch.Tensor) or logits_1v.ndim != 2:
        return 0.0
    row = logits_1v[0]
    v = int(row.shape[0])
    if v < 2:
        return 0.0
    logp = F.log_softmax(row, dim=-1)
    p = logp.exp()
    h = float(-(p * logp).sum().item())
    denom = float(np.log(float(v)))
    if denom <= 0.0:
        return 0.0
    return float(np.clip(h / denom, 0.0, 1.0))


class StreamingSigmaScaffold:
    """Collect per-generated-token scalars via a forward hook (demo signal)."""

    def __init__(self, *, tau_interrupt: float = 0.99):
        self.tau_interrupt = float(tau_interrupt)
        self.token_sigmas: List[float] = []
        self.should_stop = False
        self._hook_handle = None
        self._cognitive = SigmaState()

    def reset(self) -> None:
        self.token_sigmas.clear()
        self.should_stop = False
        self._cognitive = SigmaState()

    def _scalar_from_hidden(self, h_last: Any) -> float:
        import torch

        if not isinstance(h_last, torch.Tensor):
            return 0.5
        v = h_last.detach().float().norm().item()
        return float(np.clip(v / 200.0, 0.0, 1.0))

    def _hook_fn(self, module: Any, args: Any, output: Any) -> None:
        import torch

        hidden = output[0] if isinstance(output, tuple) else getattr(output, "last_hidden_state", output)
        if not isinstance(hidden, torch.Tensor) or hidden.ndim < 3:
            return
        last = hidden[:, -1, :]
        sigma = self._scalar_from_hidden(last)
        self.token_sigmas.append(sigma)
        sigma_update(self._cognitive, float(sigma), max(0.0, min(1.0, 1.0 - float(sigma))))
        if sigma_gate(self._cognitive) == Verdict.ABSTAIN:
            self.should_stop = True

    def register(self, model: Any, *, layer_selector: Optional[Callable[[Any], Any]] = None) -> None:
        """Attach hook to last transformer block by default (GPT-2 style ``h``)."""
        if layer_selector is not None:
            target = layer_selector(model)
        else:
            target = getattr(model, "transformer", model)
            target = getattr(target, "h", None)
            if target is not None and len(target) > 0:
                target = target[-1]
            else:
                target = model
        if self._hook_handle is not None:
            self._hook_handle.remove()
        self._hook_handle = target.register_forward_hook(self._hook_fn)

    def unregister(self) -> None:
        if self._hook_handle is not None:
            self._hook_handle.remove()
            self._hook_handle = None

    def get_sigma_trajectory(self) -> List[float]:
        return list(self.token_sigmas)


def _resolve_gpt2_style_block(model: Any, layer_idx: int) -> Any:
    """Pick a ``GPT2Block``-like module; ``layer_idx`` may be negative (from end)."""
    trans = getattr(model, "transformer", None)
    if trans is None:
        return model
    blocks = getattr(trans, "h", None)
    if blocks is None or len(blocks) == 0:
        return model
    n = len(blocks)
    i = layer_idx if layer_idx >= 0 else (n + int(layer_idx))
    i = max(0, min(int(i), n - 1))
    return blocks[i]


def _load_optional_probe(path: Optional[str | Path]) -> Any:
    if path is None:
        return None
    p = Path(path).expanduser()
    if not p.is_file():
        return None
    with p.open("rb") as f:
        return pickle.load(f)


class StreamingSigmaGate:
    """
    Greedy decode with optional forward hook (pickle probe on hidden) **or** per-step
    **normalized entropy** from logits (no hook). Halting policy depends on path: see
    module docstring (sliding-window ``tau_halt`` heuristic vs cognitive interrupt on
    probe path).
    """

    def __init__(
        self,
        probe_path: Optional[str | Path] = None,
        *,
        tau_halt: float = 0.95,
        window_size: int = 8,
        layer_idx: int = -3,
        tau_interrupt: Optional[float] = None,
        min_consecutive_high: int = 3,
        min_gen_tokens_before_halt: int = 5,
    ):
        if tau_interrupt is not None:
            tau_halt = float(tau_interrupt)
        self.tau_halt = float(tau_halt)
        self.window_size = max(1, int(window_size))
        self.layer_idx = int(layer_idx)
        self.min_consecutive_high = max(1, int(min_consecutive_high))
        self.min_gen_tokens_before_halt = max(0, int(min_gen_tokens_before_halt))
        self._probe = _load_optional_probe(probe_path)

        self.token_sigmas: List[float] = []
        self.gen_sigmas: List[float] = []
        self.should_halt = False
        self.halt_reason: Optional[str] = None
        self._hook_handle = None
        self._prompt_len: int = 0
        self._layer: Optional[Any] = None
        self._cognitive = SigmaState()
        self._verdict_traj: List[str] = []

    def _score_hidden(self, hs: np.ndarray) -> float:
        """``hs`` shape ``(1, D)`` float — pickle probe or squashed norm fallback in ``[0,1]``."""
        if self._probe is not None and hasattr(self._probe, "predict_proba"):
            try:
                pr = self._probe.predict_proba(hs.astype(np.float64))
                if pr.shape[1] >= 2:
                    return float(np.clip(pr[0, 1], 0.0, 1.0))
                return float(np.clip(float(pr.ravel()[0]), 0.0, 1.0))
            except Exception:
                pass
        v = float(np.linalg.norm(hs))
        return float(np.clip(1.0 / (1.0 + np.exp(-(v - 50.0) / 10.0)), 0.0, 1.0))

    def _apply_cognitive_step(self, sigma: float) -> None:
        sigma_update(
            self._cognitive,
            float(sigma),
            max(0.0, min(1.0, 1.0 - float(sigma))),
        )
        v = sigma_gate(self._cognitive)
        self._verdict_traj.append(v.name)
        if v == Verdict.ABSTAIN:
            self.should_halt = True
            self.halt_reason = "cognitive_interrupt: ABSTAIN"

    def _check_sliding_entropy_halt(self) -> None:
        """Halt if a run of high normalized-entropy scores appears in the trailing window."""
        g = self.gen_sigmas
        need = int(self.min_gen_tokens_before_halt) + int(self.min_consecutive_high)
        if len(g) < need:
            return
        tail = g[-int(self.window_size) :]
        m = int(self.min_consecutive_high)
        thr = float(self.tau_halt)
        for i in range(0, max(0, len(tail) - m + 1)):
            chunk = tail[i : i + m]
            if len(chunk) < m:
                break
            if all(float(x) > thr for x in chunk):
                self.should_halt = True
                self.halt_reason = f"normalized_entropy_sliding:>{thr}"
                return

    def _hook_fn(self, module: Any, args: Any, output: Any) -> None:
        import torch

        try:
            hidden = output[0] if isinstance(output, tuple) else output
            if not isinstance(hidden, torch.Tensor) or hidden.ndim < 3:
                return
            seq_len = int(hidden.shape[1])
            if seq_len <= self._prompt_len:
                return
            last = hidden[:, -1, :].detach().float().cpu().numpy()
            sigma = self._score_hidden(last.reshape(1, -1))
            self.token_sigmas.append(float(sigma))
            self.gen_sigmas.append(float(sigma))
            self._apply_cognitive_step(float(sigma))
        except Exception:
            pass

    def register(self, model: Any, *, layer_selector: Optional[Callable[[Any], Any]] = None) -> None:
        if self._probe is None:
            return
        if layer_selector is not None:
            target = layer_selector(model)
        else:
            target = _resolve_gpt2_style_block(model, self.layer_idx)
        if self._hook_handle is not None:
            self._hook_handle.remove()
        self._layer = target
        self._hook_handle = target.register_forward_hook(self._hook_fn)

    def unregister(self) -> None:
        if self._hook_handle is not None:
            self._hook_handle.remove()
            self._hook_handle = None

    def reset(self) -> None:
        self.token_sigmas.clear()
        self.gen_sigmas.clear()
        self.should_halt = False
        self.halt_reason = None
        self._cognitive = SigmaState()
        self._verdict_traj.clear()

    def generate_with_gate(
        self,
        model: Any,
        tokenizer: Any,
        prompt: str,
        *,
        max_new_tokens: int = 200,
    ) -> tuple[str, dict[str, Any]]:
        """
        Token-wise greedy generation; stops early if the sliding-window halt rule fires.

        **Heuristic (no pickle probe):** uses normalized entropy from **logits** each step
        (no forward hook). **Probe path:** uses hook on ``layer_idx`` block.
        """
        import torch

        self.reset()
        use_hook = self._probe is not None
        self.register(model)
        device = next(model.parameters()).device
        enc = tokenizer(prompt, return_tensors="pt")
        enc = {k: v.to(device) for k, v in enc.items()}
        self._prompt_len = int(enc["input_ids"].shape[1])
        generated_ids = enc["input_ids"].clone()
        attn = enc.get("attention_mask")
        if attn is None:
            attn = torch.ones_like(generated_ids, device=device)
        else:
            attn = attn.to(device)

        try:
            for _step in range(int(max_new_tokens)):
                if self.should_halt:
                    break
                with torch.no_grad():
                    out = model(input_ids=generated_ids, attention_mask=attn)
                if self.should_halt:
                    break
                if not use_hook:
                    logits = out.logits[:, -1, :]
                    sigma = normalized_entropy_from_logits(logits)
                    self.gen_sigmas.append(float(sigma))
                    self.token_sigmas.append(float(sigma))
                    self._check_sliding_entropy_halt()
                    if self.should_halt:
                        break
                next_token = out.logits[:, -1, :].argmax(dim=-1, keepdim=True)
                generated_ids = torch.cat([generated_ids, next_token], dim=1)
                attn = torch.cat(
                    [attn, torch.ones((attn.shape[0], 1), dtype=attn.dtype, device=device)],
                    dim=1,
                )
                eos = tokenizer.eos_token_id
                if eos is not None and int(next_token.item()) == int(eos):
                    break
        finally:
            self.unregister()

        response_ids = generated_ids[0, self._prompt_len :]
        response = tokenizer.decode(response_ids, skip_special_tokens=True).strip()
        n_gen = int(response_ids.shape[0])
        meta = {
            "tokens_generated": n_gen,
            "tokens_max": int(max_new_tokens),
            "halted": bool(self.should_halt),
            "halt_reason": self.halt_reason,
            "token_sigmas": list(self.gen_sigmas),
            "mean_sigma": float(np.mean(self.gen_sigmas)) if self.gen_sigmas else 0.0,
            "max_sigma": float(np.max(self.gen_sigmas)) if self.gen_sigmas else 0.0,
            "tokens_saved": max(0, int(max_new_tokens) - n_gen) if self.should_halt else 0,
            "heuristic": "normalized_entropy_over_log_vocab" if not use_hook else "pickle_probe_on_hidden",
            "cognitive_verdicts": list(self._verdict_traj),
            "rethink_steps": int(sum(1 for x in self._verdict_traj if x == "RETHINK")),
            "tau_halt": self.tau_halt,
            "window_size": self.window_size,
            "min_consecutive_high": self.min_consecutive_high,
            "halt_policy": "sliding_normalized_entropy" if not use_hook else "pickle_probe_cognitive",
        }
        return response, meta


__all__ = [
    "StreamingSigmaScaffold",
    "StreamingSigmaGate",
    "normalized_entropy_from_logits",
]
