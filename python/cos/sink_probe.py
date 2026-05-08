# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Attention sink probe (lab) — column-mass concentration per token + mid-layer compression cue.

This is a **numpy/torch-free** sketch for integrators. It does **not** reproduce external
benchmarks named in marketing copy; wire real maps from your stack. See
``docs/CLAIM_DISCIPLINE.md``.

Integrates as **L6** via :func:`cos.cascade.cascade_L6`.
"""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import math
from typing import Any, Dict, List

__all__ = ["SigmaSinkProbe"]


class SigmaSinkProbe:
    """Training-free signals from attention maps (and optional layer norms of hidden states)."""

    @property
    def training_free(self) -> bool:
        return True

    def sink_score(self, attention_maps: Any) -> Dict[str, Any]:
        """Per-token incoming attention mass (column sums); outliers vs mean = sink intensity."""
        layers = self._normalize_attention_maps(attention_maps)
        if not layers:
            return {"per_token": [], "aggregate": 0.5, "n_tokens": 0}
        n_tok = len(layers[0][0])
        sums: List[float] = [0.0] * n_tok
        n_heads = 0
        for layer in layers:
            for head in layer:
                n_heads += 1
                for j in range(n_tok):
                    sums[j] += sum(float(head[i][j]) for i in range(n_tok))
        if n_heads <= 0:
            return {"per_token": [0.0] * n_tok, "aggregate": 0.5, "n_tokens": n_tok}
        avg_col = [s / float(n_heads) for s in sums]
        mu = sum(avg_col) / max(n_tok, 1)
        var = sum((x - mu) ** 2 for x in avg_col) / max(n_tok, 1)
        std = math.sqrt(var) or 1e-9
        per_tok = [min(1.0, max(0.0, abs(x - mu) / (2.0 * std))) for x in avg_col]
        agg = max(per_tok) if per_tok else 0.5
        return {"per_token": per_tok, "aggregate": float(agg), "n_tokens": n_tok}

    def compression_valley_detect(self, hidden_states: Any) -> Dict[str, Any]:
        """Heuristic minimum Δ-norm between consecutive layer mean vectors (compression valley)."""
        hs = self._layer_norms(hidden_states)
        if len(hs) < 3:
            return {"index": -1, "score": 0.0, "deltas": []}
        deltas: List[float] = []
        for i in range(len(hs) - 1):
            deltas.append(abs(float(hs[i + 1]) - float(hs[i])))
        if len(deltas) > 2:
            inner = deltas[1:-1]
            mid = 1 + inner.index(min(inner))
            vmin = min(inner)
            vmax = max(inner) or 1e-9
        else:
            mid = int(deltas.index(min(deltas)))
            vmin = min(deltas)
            vmax = max(deltas) or 1e-9
        depth = 1.0 - vmin / max(vmax, 1e-9)
        return {
            "index": mid,
            "score": float(min(1.0, max(0.0, depth))),
            "deltas": [round(d, 6) for d in deltas],
        }

    def combined_with_spectral(self, attention_maps: Any) -> Dict[str, Any]:
        """Sink aggregate + rough Laplacian largest-eigenvalue proxy on mean attention."""
        sk = self.sink_score(attention_maps)
        lam = self._spectral_radius_attention(attention_maps)
        combo = min(1.0, 0.65 * float(sk["aggregate"]) + 0.35 * float(lam))
        return {"sink_aggregate": sk["aggregate"], "spectral_stress": lam, "combined": combo, "per_token": sk["per_token"]}

    @staticmethod
    def _layer_norms(hidden_states: Any) -> List[float]:
        if hidden_states is None:
            return []
        if hasattr(hidden_states, "shape"):
            return []  # torch tensor — lab integrator should pass list
        out: List[float] = []
        for layer in hidden_states:
            if layer is None:
                continue
            if hasattr(layer, "detach"):
                try:

                    t = layer.detach().float().cpu()
                    out.append(float(t.abs().mean()))
                except Exception:  # pragma: no cover
                    out.append(0.0)
            elif isinstance(layer, (list, tuple)):
                flat = [float(x) for row in layer for x in (row if isinstance(row, (list, tuple)) else [row])]
                out.append(sum(abs(x) for x in flat) / max(len(flat), 1))
            else:
                out.append(0.0)
        return out

    @staticmethod
    def _normalize_attention_maps(attention_maps: Any) -> List[List[List[List[float]]]]:
        if attention_maps is None:
            return []
        # torch tensor [L,H,Q,K]
        if hasattr(attention_maps, "shape") and hasattr(attention_maps, "cpu"):
            try:

                t = attention_maps.detach().float().cpu()
                if t.dim() != 4:
                    return []
                L, H, Q, K = int(t.shape[0]), int(t.shape[1]), int(t.shape[2]), int(t.shape[3])
                return [
                    [
                        [[float(t[li, hi, qi, ki]) for ki in range(K)] for qi in range(Q)]
                        for hi in range(H)
                    ]
                    for li in range(L)
                ]
            except Exception:  # pragma: no cover
                return []

        layers: List[List[List[List[float]]]] = []
        if isinstance(attention_maps, (list, tuple)):
            for layer in attention_maps:
                if layer is None:
                    continue
                heads: List[List[List[float]]] = []
                if isinstance(layer, (list, tuple)) and layer and isinstance(layer[0], (list, tuple)):
                    for head in layer:
                        mat = [[float(x) for x in row] for row in head]  # type: ignore[union-attr]
                        heads.append(mat)
                layers.append(heads)
        return layers

    def _spectral_radius_attention(self, attention_maps: Any, *, iters: int = 8) -> float:
        layers = self._normalize_attention_maps(attention_maps)
        if not layers:
            return 0.0
        n = len(layers[0][0])
        acc = [[0.0] * n for _ in range(n)]
        nh = 0
        for layer in layers:
            for head in layer:
                nh += 1
                for i in range(n):
                    for j in range(n):
                        acc[i][j] += float(head[i][j])
        if nh <= 0:
            return 0.0
        for i in range(n):
            for j in range(n):
                acc[i][j] /= float(nh)
        sym: List[List[float]] = []
        for i in range(n):
            row = []
            for j in range(n):
                row.append(0.5 * (acc[i][j] + acc[j][i]))
            sym.append(row)
        deg = [sum(sym[i]) for i in range(n)]
        lap = [[0.0] * n for _ in range(n)]
        for i in range(n):
            for j in range(n):
                lap[i][j] = (deg[i] if i == j else 0.0) - sym[i][j]
        v = [1.0 / n**0.5] * n
        lam = 0.0
        for _ in range(iters):
            w = [sum(lap[i][j] * v[j] for j in range(n)) for i in range(n)]
            norm = math.sqrt(sum(x * x for x in w)) or 1.0
            v = [x / norm for x in w]
            lam = sum(v[i] * sum(lap[i][j] * v[j] for j in range(n)) for i in range(n))
        return float(min(1.0, max(0.0, abs(lam) / max(n, 1) ** 0.5)))
