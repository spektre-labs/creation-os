# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
σ-attention (lab): sub-quadratic attention by applying full pairwise weights only on **low-σ**
query rows; high-σ rows use a sliding window.

Python implementation mirrors ``src/inference/sigma_attention.c`` semantics for CI / CLI; it is not
FlashAttention and does not fuse GPU kernels.
"""
from __future__ import annotations

import math
import time
from typing import Any, Dict, List, Optional, Protocol, Sequence, Tuple

COS_ATTN_Q16 = 65536


class TokenSigmaGate(Protocol):
    def estimate_token_sigma(self, index: int, *, seq_len: int) -> float:
        """Per-position σ in [0, 1] — lower means “trust this token more” (full attention)."""


class LabTokenSigmaGate:
    """Deterministic demo: ~``low_fraction`` of indices get low σ (full attention candidates)."""

    def __init__(self, seq_len: int, *, low_fraction: float = 0.12, low_sigma: float = 0.08, high_sigma: float = 0.72):
        self.seq_len = int(seq_len)
        self.low_fraction = float(low_fraction)
        self.low_sigma = float(low_sigma)
        self.high_sigma = float(high_sigma)

    def estimate_token_sigma(self, index: int, *, seq_len: int) -> float:
        n = max(1, int(seq_len))
        cutoff = max(1, int(math.floor(self.low_fraction * n)))
        if int(index) < cutoff:
            return self.low_sigma
        return self.high_sigma


def _dot_rows(a: Sequence[float], b: Sequence[float]) -> float:
    return sum(float(x) * float(y) for x, y in zip(a, b))


def _attend_row(
    q_row: Sequence[float],
    k_rows: Sequence[Sequence[float]],
    v_rows: Sequence[Sequence[float]],
    j_start: int,
    j_end: int,
) -> List[float]:
    d = len(q_row)
    acc = [0.0] * d
    sumw = 0.0
    for j in range(max(0, j_start), min(len(k_rows), j_end)):
        s = _dot_rows(q_row, k_rows[j])
        w = max(0.0, s) + 1.0
        sumw += w
        for k in range(d):
            acc[k] += w * float(v_rows[j][k])
    if sumw <= 0.0:
        return [0.0] * d
    return [acc[k] / sumw for k in range(d)]


class SigmaAttention:
    def __init__(
        self,
        gate: Optional[Any] = None,
        *,
        window_size: int = 128,
        sigma_threshold: float = 0.3,
    ) -> None:
        self.gate = gate
        self.window = max(1, int(window_size))
        self.threshold = float(sigma_threshold)
        self.stats: Dict[str, int] = {"full": 0, "pruned": 0}

    def forward(
        self,
        q: Sequence[Sequence[float]],
        k: Sequence[Sequence[float]],
        v: Sequence[Sequence[float]],
        *,
        token_sigmas: Optional[Sequence[float]] = None,
    ) -> List[List[float]]:
        seq_len = len(q)
        g = self.gate
        self.stats = {"full": 0, "pruned": 0}
        sigs: List[float]
        if token_sigmas is not None:
            sigs = [float(x) for x in token_sigmas[:seq_len]]
            while len(sigs) < seq_len:
                sigs.append(0.5)
        else:
            if g is None:
                g = LabTokenSigmaGate(seq_len)
            sigs = [float(g.estimate_token_sigma(i, seq_len=seq_len)) for i in range(seq_len)]

        thr = self.threshold
        out: List[List[float]] = []
        for i in range(seq_len):
            qs = float(sigs[i])
            if qs < thr:
                out.append(_attend_row(q[i], k, v, 0, seq_len))
                self.stats["full"] += 1
            else:
                lo = max(0, i - self.window)
                hi = min(seq_len, i + self.window + 1)
                out.append(_attend_row(q[i], k, v, lo, hi))
                self.stats["pruned"] += 1
        return out

    def complexity(self, seq_len: int) -> Dict[str, Any]:
        total = self.stats["full"] + self.stats["pruned"]
        if total <= 0:
            return {"estimated_savings": "0%", "full_ratio": 0.0, "full_tokens": 0, "pruned_tokens": 0}
        fr = self.stats["full"] / float(total)
        eff = fr * float(seq_len) + (1.0 - fr) * float(2 * self.window + 1)
        baseline = float(seq_len)
        saving = 0.0
        if baseline > 1e-9:
            saving = max(0.0, 100.0 * (1.0 - eff / baseline))
        return {
            "effective_key_span_per_row": round(eff, 4),
            "full_ratio": round(fr, 6),
            "full_tokens": int(self.stats["full"]),
            "pruned_tokens": int(self.stats["pruned"]),
            "estimated_savings_vs_dense_n": f"{saving:.0f}% sub-quadratic pattern",
        }


def dense_attention_reference(q: Sequence[Sequence[float]], k: Sequence[Sequence[float]], v: Sequence[Sequence[float]]) -> List[List[float]]:
    """O(N²) reference (same weighting rule as :meth:`SigmaAttention.forward` full rows)."""
    n = len(q)
    return [_attend_row(q[i], k, v, 0, n) for i in range(n)]


def benchmark_sigma_vs_dense(
    seq_len: int,
    head_dim: int,
    *,
    window: int = 128,
    threshold: float = 0.3,
    low_fraction: float = 0.12,
    repeats: int = 2,
) -> Dict[str, Any]:
    """Wall-clock lab ratio; not hardware FlashAttention."""
    n = max(2, int(seq_len))
    d = max(1, int(head_dim))
    q = [[float((i + k) % 17) / 16.0 for k in range(d)] for i in range(n)]
    kk = [[float((i * k + 3) % 19) / 18.0 for k in range(d)] for i in range(n)]
    vv = [[float((i + k * k) % 13) / 12.0 for k in range(d)] for i in range(n)]

    gate = LabTokenSigmaGate(n, low_fraction=low_fraction)
    attn = SigmaAttention(gate, window_size=window, sigma_threshold=threshold)

    t0 = time.perf_counter()
    for _ in range(repeats):
        dense_attention_reference(q, kk, vv)
    t_dense = max(1e-12, (time.perf_counter() - t0) / float(max(1, repeats)))

    t1 = time.perf_counter()
    for _ in range(repeats):
        attn.forward(q, kk, vv)
    t_sparse = max(1e-12, (time.perf_counter() - t1) / float(max(1, repeats)))

    pruned_ratio = attn.stats["pruned"] / float(n)
    return {
        "dense_ms_per_run": round(t_dense * 1000.0, 4),
        "head_dim": d,
        "pruned_tokens": int(attn.stats["pruned"]),
        "seq_len": n,
        "sparse_ms_per_run": round(t_sparse * 1000.0, 4),
        "speedup": round(t_dense / t_sparse, 4),
        "full_tokens": int(attn.stats["full"]),
        "pruned_token_ratio": round(pruned_ratio, 4),
    }


def kv_prune_rows(
    kv_rows: List[List[float]],
    sigmas: List[float],
    *,
    max_cache: int,
) -> Tuple[Tuple[List[List[float]], List[float]], int]:
    """
    Drop one highest-σ cache row (swap with last), mirroring ``cos_infer_sigma_kv_prune``.
    Each ``kv_rows[i]`` is one flattened row ``K_i || V_i`` (2 * head_dim floats).
    Returns ``(new_kv, new_sigmas), pruned_count`` (single step).
    """
    if len(kv_rows) != len(sigmas) or len(kv_rows) <= max_cache:
        return (kv_rows, sigmas), 0
    worst = 0
    worst_s = float(sigmas[0])
    for i in range(1, len(sigmas)):
        if float(sigmas[i]) > worst_s:
            worst_s = float(sigmas[i])
            worst = i
    last = len(kv_rows) - 1
    kv = [list(r) for r in kv_rows]
    sg = list(sigmas)
    if worst != last:
        kv[worst], kv[last] = kv[last], kv[worst]
        sg[worst], sg[last] = sg[last], sg[worst]
    return (kv[:-1], sg[:-1]), 1


__all__ = [
    "COS_ATTN_Q16",
    "LabTokenSigmaGate",
    "SigmaAttention",
    "benchmark_sigma_vs_dense",
    "dense_attention_reference",
    "kv_prune_rows",
]
