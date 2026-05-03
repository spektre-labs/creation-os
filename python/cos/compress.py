# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-aware compression **lab** — quantize → prune → distill → verify story (no torch required).

Connects conceptually to :mod:`cos.sigma_quantize`, distillation CLI, and BitNet paths; tensors
are plain dict/list stand-ins here. See ``docs/CLAIM_DISCIPLINE.md``.
"""
from __future__ import annotations

import copy
import math
from typing import Any, Dict, List, MutableMapping, Sequence, Tuple

__all__ = ["SigmaCompress"]

ModelDict = MutableMapping[str, List[float]]


class SigmaCompress:
    """Toy compression pipeline reporting σ drift on probe strings."""

    METHODS = ("quantize", "distill", "prune", "merge")

    def compress(
        self,
        model: ModelDict,
        gate: Any,
        target_size: int,
    ) -> Dict[str, Any]:
        """Reduce parameter count toward ``target_size`` (element count); return σ report."""
        cur = sum(len(v) for v in model.values())
        if cur <= target_size:
            report = {"stages": [], "sigma_max": 0.0, "final_elements": cur}
            return {"compressed": copy.deepcopy(model), "report": report}
        stages: List[str] = []
        m2 = copy.deepcopy(model)
        # Quantize/coarsen: round to 0.01
        for k in list(m2.keys()):
            m2[k] = [round(float(x), 2) for x in m2[k]]
        stages.append("quantize")
        cur = sum(len(v) for v in m2.values())
        # Prune tail weights per layer if still too big
        if cur > target_size:
            for k in list(m2.keys()):
                need_drop = max(0, len(m2[k]) - max(1, target_size // max(len(m2), 1)))
                if need_drop:
                    m2[k] = m2[k][:-need_drop]
            stages.append("prune")
        cur = sum(len(v) for v in m2.values())
        sigma_max = 0.0
        for k, vec in m2.items():
            preview = " | ".join(f"{x:.4f}" for x in vec[:8])
            s = float(gate.compute_sigma(None, None, f"compress:{k}", preview))
            sigma_max = max(sigma_max, s)
        return {
            "compressed": m2,
            "report": {
                "stages": stages + ["verify"],
                "sigma_max": round(sigma_max, 6),
                "final_elements": cur,
                "target_size": int(target_size),
            },
        }

    def sigma_before_after(
        self,
        original: ModelDict,
        compressed: ModelDict,
        test_data: Sequence[str],
        gate: Any,
    ) -> Dict[str, Any]:
        """Mean |Δσ| over probe texts using model string previews (lab)."""
        def _blob(m: ModelDict) -> str:
            parts: List[str] = []
            for k, vec in sorted(m.items()):
                parts.append(f"{k}:{len(vec)}")
            return ";".join(parts)

        rows = []
        for t in test_data:
            pre_o = float(gate.compute_sigma(None, None, str(t), _blob(original)))
            pre_c = float(gate.compute_sigma(None, None, str(t), _blob(compressed)))
            rows.append({"delta": round(pre_c - pre_o, 6), "sigma_before": pre_o, "sigma_after": pre_c})
        mean_delta = sum(abs(r["delta"]) for r in rows) / max(len(rows), 1)
        return {"mean_abs_delta_sigma": round(mean_delta, 6), "rows": rows}

    @staticmethod
    def compression_budget(sigma_after: float, *, threshold: float = 0.45) -> Dict[str, Any]:
        """Return whether compressed artefact stays under σ ceiling."""
        ok = float(sigma_after) <= float(threshold)
        return {"within_budget": ok, "sigma": float(sigma_after), "threshold": float(threshold)}

    def pareto_frontier(
        self,
        models: Sequence[Tuple[ModelDict, str]],
        test_data: Sequence[str],
        gate: Any,
    ) -> List[Dict[str, Any]]:
        """Non-dominated size vs mean σ (lower better for both)."""
        pts: List[Dict[str, Any]] = []
        for m, label in models:
            sz = sum(len(v) for v in m.values())
            sigmas: List[float] = []
            for t in test_data:
                blob = str(sorted((k, len(v)) for k, v in m.items()))
                sigmas.append(float(gate.compute_sigma(None, None, str(t), blob)))
            mean_s = sum(sigmas) / max(len(sigmas), 1)
            pts.append({"label": label, "size": sz, "mean_sigma": round(mean_s, 6)})
        pts.sort(key=lambda x: (x["size"], x["mean_sigma"]))
        frontier: List[Dict[str, Any]] = []
        best_sigma = math.inf
        for p in pts:
            if p["mean_sigma"] < best_sigma:
                frontier.append(p)
                best_sigma = p["mean_sigma"]
        return frontier
