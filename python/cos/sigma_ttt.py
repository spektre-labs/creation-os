# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Test-time training (**TTT**) scaffold gated by ``sigma_gate_core``.

**v123 extensions:** in-place style updates on a final linear projection, **chunk-wise**
micro-batches, **σ critic** (rollback when σ rises after an update), and optional **Engram**
snapshots (JSON sidecar — lab only).

**v150 extensions:** gate-guided test-time adaptation lives in :mod:`cos.sigma_ttt_v2` (torch-free
API: :class:`cos.sigma_ttt_v2.SigmaTTTv2`) so hosts without PyTorch can still run the v150 loop + CLI.

**Orientation:** Complements in-context loops; not ByteDance In-Place-TTT / TEMPO reproduction.
See ``docs/CLAIM_DISCIPLINE.md``.

``sigma_gate.h`` remains authoritative for embedded interrupts; this module is Python-lab only.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

try:
    import torch
    import torch.nn as nn
    _TorchBase: type = nn.Module
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[assignment]
    nn = None  # type: ignore[assignment]
    _TorchBase = object


from .sigma_gate_core import SigmaState, Verdict, sigma_gate, sigma_update


def _require_torch_ttt() -> None:
    if torch is None or nn is None:
        raise ImportError(
            "cos.sigma_ttt requires PyTorch — install e.g. pip install torch",
        ) from None


class SigmaTTT(_TorchBase):
    """Low-rank delta on activations; TTT step only on ``RETHINK``."""

    def __init__(self, *, d_model: int, lr: float = 1e-2) -> None:
        _require_torch_ttt()
        super().__init__()
        self.d_model = int(d_model)
        self.lr = float(lr)
        self.delta = nn.Parameter(torch.zeros(self.d_model, self.d_model))

    def compute_ttt_loss(self, x: torch.Tensor, output: torch.Tensor) -> torch.Tensor:
        adj = x @ self.delta
        return (output - adj).pow(2).mean()

    def update_last_layer(self, loss: torch.Tensor) -> None:
        grad = torch.autograd.grad(loss, self.delta, retain_graph=False, create_graph=False)[0]
        with torch.no_grad():
            self.delta -= self.lr * grad

    def forward(
        self,
        x: torch.Tensor,
        state: SigmaState,
        *,
        backbone: Callable[[torch.Tensor], torch.Tensor],
        probe_sigma: float,
        k_raw: float = 0.7,
    ) -> Tuple[Optional[torch.Tensor], Verdict]:
        output = backbone(x)
        sigma_update(state, float(probe_sigma), float(k_raw))
        verdict = sigma_gate(state)
        if verdict == Verdict.RETHINK:
            loss = self.compute_ttt_loss(x, output.detach())
            self.update_last_layer(loss)
            output = backbone(x)
            sigma_update(state, float(probe_sigma), float(k_raw))
            verdict = sigma_gate(state)
        if verdict == Verdict.ABSTAIN:
            return None, verdict
        return output, verdict


class SigmaTTTInPlace(nn.Module):
    """
    Drop-in style **fast projection**: maintains ``W_frozen`` + ``delta`` applied as
    ``x @ (W + Δ)`` in the last linear (lab simplification — no full MLP stack).
    """

    def __init__(self, *, d_in: int, d_out: int, lr: float = 5e-3) -> None:
        _require_torch_ttt()
        super().__init__()
        self.d_in = int(d_in)
        self.d_out = int(d_out)
        self.lr = float(lr)
        self.W = nn.Parameter(torch.randn(d_out, d_in) * 0.02)
        self.delta = nn.Parameter(torch.zeros(d_out, d_in))
        self.bias = nn.Parameter(torch.zeros(d_out))
        self.W.requires_grad_(False)
        self.bias.requires_grad_(False)

    def proj_weight(self) -> torch.Tensor:
        return self.W + self.delta

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return torch.nn.functional.linear(x, self.proj_weight(), self.bias)

    def chunk_train_step(
        self,
        chunks: List[torch.Tensor],
        target_factory: Callable[[torch.Tensor], torch.Tensor],
    ) -> float:
        """One manual gradient step across chunks (mean squared error to target)."""
        total_loss: Optional[torch.Tensor] = None
        self.zero_grad(set_to_none=True)
        for ch in chunks:
            y_hat = self.forward(ch)
            y = target_factory(ch)
            loss = (y_hat - y).pow(2).mean()
            total_loss = loss if total_loss is None else total_loss + loss
        if total_loss is None:
            return 0.0
        total_loss = total_loss / max(1, len(chunks))
        total_loss.backward()
        with torch.no_grad():
            self.delta -= self.lr * self.delta.grad  # type: ignore[union-attr]
        self.delta.grad = None
        return float(total_loss.detach().item())


def _prompt_embedding(prompt: str, context: Optional[str], dim: int) -> torch.Tensor:
    _require_torch_ttt()
    raw = f"{prompt}\n{context or ''}".encode("utf-8", errors="ignore")
    vec = [0.0] * dim
    for i, b in enumerate(raw[: dim * 4]):
        vec[i % dim] += float(b) / 255.0
    t = torch.tensor(vec, dtype=torch.float32)
    return t / (t.norm() + 1e-8)


def _probe_sigma_from_prompt(prompt: str, context: Optional[str]) -> float:
    h = hashlib.sha256(f"{prompt}|{context or ''}".encode()).digest()
    return int.from_bytes(h[:2], "big") / 65535.0


def _split_chunks(x: torch.Tensor, chunk_size: int) -> List[torch.Tensor]:
    if x.dim() == 1:
        x = x.unsqueeze(0)
    n = x.shape[0]
    cs = max(1, int(chunk_size))
    out: List[torch.Tensor] = []
    for i in range(0, n, cs):
        out.append(x[i : i + cs])
    return out if out else [x]


class SigmaGatedTTTOrchestrator:
    """
    σ-gated TTT orchestrator: update **only** on ``RETHINK``; skip on ``ACCEPT``;
    **no learn** on ``ABSTAIN``. Uses σ before/after as critic signal.
    """

    def __init__(
        self,
        *,
        dim: int = 16,
        chunk_size: int = 64,
        max_steps: int = 5,
        k_raw: float = 0.88,
        lr: float = 5e-3,
        engram_path: Optional[Path] = None,
    ) -> None:
        _require_torch_ttt()
        self.dim = int(dim)
        self.chunk_size = int(chunk_size)
        self.max_steps = int(max_steps)
        self.k_raw = float(k_raw)
        self.module = SigmaTTTInPlace(d_in=dim, d_out=dim, lr=lr)
        self.state = SigmaState()
        self.last_sigma_before = 0.0
        self.last_sigma_after = 0.0
        self._engram_path = engram_path

    def _load_engram_delta(self, key: str) -> None:
        if self._engram_path is None or not self._engram_path.is_file():
            return
        try:
            blob = json.loads(self._engram_path.read_text(encoding="utf-8"))
            row = blob.get(key)
            if isinstance(row, dict) and "delta" in row:
                t = torch.tensor(row["delta"], dtype=torch.float32)
                if t.shape == self.module.delta.shape:
                    with torch.no_grad():
                        self.module.delta.copy_(t)
        except (OSError, json.JSONDecodeError, TypeError):
            return

    def _save_engram_delta(self, key: str) -> None:
        if self._engram_path is None:
            return
        self._engram_path.parent.mkdir(parents=True, exist_ok=True)
        data: Dict[str, Any] = {}
        if self._engram_path.is_file():
            try:
                data = json.loads(self._engram_path.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                data = {}
        data[key] = {
            "delta": self.module.delta.detach().cpu().tolist(),
            "dim": self.dim,
        }
        self._engram_path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    def inference_with_learning(
        self,
        prompt: str,
        context: Optional[str] = None,
        *,
        probe_sigma: Optional[float] = None,
    ) -> Tuple[str, float, str]:
        key = hashlib.sha256(f"{prompt}|{context or ''}".encode()).hexdigest()[:24]
        self._load_engram_delta(key)

        self.state = SigmaState()
        x = _prompt_embedding(prompt, context, self.dim)
        ps = float(probe_sigma) if probe_sigma is not None else float(_probe_sigma_from_prompt(prompt, context))

        sigma_update(self.state, ps, self.k_raw)
        verdict = sigma_gate(self.state)
        self.last_sigma_before = float(ps)

        if verdict == Verdict.ABSTAIN:
            self.last_sigma_after = float(ps)
            return "", float(ps), "ABSTAIN"

        if verdict != Verdict.RETHINK:
            y = self.module(x.unsqueeze(0))
            self.last_sigma_after = float(torch.tanh(y.norm()).item())
            self._save_engram_delta(key)
            return f"lab_out:{float(y.norm().item()):.4f}", self.last_sigma_after, "ACCEPT"

        backup = self.module.delta.detach().clone()
        chunks = _split_chunks(x.unsqueeze(0), self.chunk_size)
        sigma_track = float(ps)

        for _ in range(self.max_steps):

            def _target(ch: torch.Tensor) -> torch.Tensor:
                return torch.tanh(torch.nn.functional.linear(ch, self.module.W, self.module.bias))

            loss_val = self.module.chunk_train_step(chunks, _target)
            y = self.module(x.unsqueeze(0))
            after = float(torch.tanh(y.norm()).item())
            if after <= sigma_track + 1e-6:
                sigma_track = after
                continue
            with torch.no_grad():
                self.module.delta.copy_(backup)
            _ = loss_val
            break

        self.last_sigma_after = sigma_track
        y2 = self.module(x.unsqueeze(0))
        self._save_engram_delta(key)
        return f"lab_ttt:{float(y2.norm().item()):.4f}", self.last_sigma_after, "RETHINK"

    def eval_before_after(self, prompt: str, context: Optional[str] = None) -> Dict[str, Any]:
        out, _saft, tag = self.inference_with_learning(prompt, context)
        return {
            "output": out,
            "sigma_before": float(self.last_sigma_before),
            "sigma_after": float(self.last_sigma_after),
            "tag": tag,
            "improved": bool(self.last_sigma_after <= self.last_sigma_before + 1e-6),
        }


def build_sigma_gated_ttt_lab(
    *,
    dim: int = 16,
    chunk_size: int = 64,
    max_steps: int = 5,
    engram_path: Optional[Path] = None,
) -> Tuple[SigmaGatedTTTOrchestrator, SigmaTTTInPlace, SigmaState]:
    """
    ``engram_path=None`` disables Engram JSON load/save (use for ephemeral eval).

    Pass ``Path.home() / ".cos" / "ttt_engram_lab.json"`` (or ``--engram-file``) to persist ``delta``.
    """
    orch = SigmaGatedTTTOrchestrator(dim=dim, chunk_size=chunk_size, max_steps=max_steps, engram_path=engram_path)
    return orch, orch.module, orch.state


__all__ = [
    "SigmaGatedTTTOrchestrator",
    "SigmaTTT",
    "SigmaTTTInPlace",
    "build_sigma_gated_ttt_lab",
]
