# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
Inference-time and **persistent-edit-shaped** σ steering on SAE latents / activations.

**Lab scaffolding:** faithful / hallucinatory direction tweaks resemble training-free
steering literature; correlation helpers resemble validation-by-intervention patterns.
Do **not** claim reproduce paper metrics here — see ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence, Tuple

try:
    import torch
except ImportError:  # pragma: no cover
    torch = None  # type: ignore[misc, assignment]

from .sigma_sae import SigmaSAE


def _require_torch() -> Any:
    if torch is None:
        raise ImportError("sigma_steering requires PyTorch (`pip install torch`).")
    return torch


def _pearson(xs: Sequence[float], ys: Sequence[float]) -> float:
    n = len(xs)
    if n != len(ys) or n < 2:
        return 0.0
    mx = sum(xs) / n
    my = sum(ys) / n
    num = sum((float(x) - mx) * (float(y) - my) for x, y in zip(xs, ys))
    dx = sum((float(x) - mx) ** 2 for x in xs) ** 0.5
    dy = sum((float(y) - my) ** 2 for y in ys) ** 0.5
    if dx <= 1e-12 or dy <= 1e-12:
        return 0.0
    return num / (dx * dy)


class SigmaSteering:
    """SSL-shaped direction steering + CorrSteer-shaped correlation + SALVE-shaped recurrence ledger."""

    def __init__(self) -> None:
        self._halluc_hits: Dict[int, int] = {}

    def inference_direction_steering(
        self,
        hidden_states: Any,
        *,
        faithful_direction: Optional[Any] = None,
        hallucinatory_direction: Optional[Any] = None,
        scale_faithful: float = 0.0,
        scale_halluc_down: float = 0.0,
    ) -> Any:
        """
        ``h ← h + s_f u_f - s_h proj(h, u_h)`` with normalized ``u_*`` when provided.

        Missing directions are treated as **no-op** on that axis.
        """
        t = _require_torch()
        if not isinstance(hidden_states, t.Tensor):
            raise TypeError("hidden_states must be a torch.Tensor.")
        h = hidden_states.detach().float()
        out = h.clone()

        if faithful_direction is not None:
            uf = faithful_direction
            if not isinstance(uf, t.Tensor):
                raise TypeError("faithful_direction must be a torch.Tensor.")
            uf = uf.detach().float().reshape(-1)
            hflat = h.reshape(-1)
            if uf.numel() != hflat.numel():
                raise ValueError("faithful_direction must match hidden size.")
            uf = uf / (t.linalg.norm(uf) + 1e-8)
            out = out.reshape(-1) + float(scale_faithful) * uf
            out = out.reshape_as(h)

        if hallucinatory_direction is not None and scale_halluc_down != 0.0:
            uh = hallucinatory_direction
            if not isinstance(uh, t.Tensor):
                raise TypeError("hallucinatory_direction must be a torch.Tensor.")
            uh = uh.detach().float().reshape(-1)
            hflat = out.reshape(-1)
            if uh.numel() != hflat.numel():
                raise ValueError("hallucinatory_direction must match hidden size.")
            uh = uh / (t.linalg.norm(uh) + 1e-8)
            proj = (hflat * uh).sum() * uh
            out = (hflat - float(scale_halluc_down) * proj).reshape_as(out)

        return out.type_as(hidden_states)

    def correlate_sigma_with_activations(
        self,
        sigma_series: Sequence[float],
        feature_rows: Sequence[Any],
    ) -> List[Dict[str, Any]]:
        """
        Pearson correlation of a per-step σ series with each SAE coordinate (last-axis slice).

        ``feature_rows[t]`` may be ``[F]`` tensor or flat list of length ``F``.
        """
        t = _require_torch()
        if not feature_rows:
            return []
        xs = [float(x) for x in sigma_series]
        # Build [T, F]
        mat: List[List[float]] = []
        for row in feature_rows:
            if isinstance(row, t.Tensor):
                z = row.detach().float().reshape(-1)
                vec = [float(z[i].item()) for i in range(int(z.numel()))]
            else:
                vec = [float(x) for x in row]
            mat.append(vec)
        f = len(mat[0])
        if any(len(r) != f for r in mat):
            raise ValueError("feature_rows must share dictionary width.")
        if len(mat) != len(xs):
            raise ValueError("sigma_series and feature_rows must have same length.")
        out: List[Dict[str, Any]] = []
        for j in range(f):
            col = [mat[i][j] for i in range(len(mat))]
            out.append({"feature_index": j, "pearson_r": round(_pearson(xs, col), 6)})
        return out

    def record_hallucination_hit(self, feature_idx: int) -> int:
        k = int(feature_idx)
        self._halluc_hits[k] = self._halluc_hits.get(k, 0) + 1
        return self._halluc_hits[k]

    def suggest_permanent_patch(
        self,
        feature_idx: int,
        *,
        min_hits: int = 3,
        alpha_crit: float = 0.15,
        sigma_drop_if_ablated: float = 0.0,
    ) -> Optional[Dict[str, Any]]:
        """
        SALVE-shaped stub: emit a patch hint when a coordinate fires often **and**
        an external ablation study reports ``sigma_drop_if_ablated ≥ alpha_crit``.
        """
        if self._halluc_hits.get(int(feature_idx), 0) < int(min_hits):
            return None
        if float(sigma_drop_if_ablated) < float(alpha_crit):
            return None
        return {
            "feature_idx": int(feature_idx),
            "hits": int(self._halluc_hits[int(feature_idx)]),
            "sigma_drop_if_ablated": float(sigma_drop_if_ablated),
            "alpha_crit": float(alpha_crit),
            "note": "Wire weight surgery / LoRA in harness; this object tracks recurrence only.",
        }

    def steer_with_sigma_sae(
        self,
        sae: SigmaSAE,
        hidden_states: Any,
        *,
        faithful_direction: Optional[Any] = None,
        hallucinatory_direction: Optional[Any] = None,
        scale_faithful: float = 0.0,
        scale_halluc_down: float = 0.0,
    ) -> Dict[str, Any]:
        """Steer activations then report aggregate σ and clarify mass on new latents."""
        h2 = self.inference_direction_steering(
            hidden_states,
            faithful_direction=faithful_direction,
            hallucinatory_direction=hallucinatory_direction,
            scale_faithful=scale_faithful,
            scale_halluc_down=scale_halluc_down,
        )
        agg, z, meta = sae.aggregate_sigma_sae(h2)
        clar = sae.clarify_scores(z)
        return {"hidden_steered": h2, "sigma_sae": float(agg), "meta": meta, "clarify": clar}


__all__ = ["SigmaSteering"]
