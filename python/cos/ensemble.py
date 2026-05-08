# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Probe ensemble (lab) — fuse multiple scalar probe channels with weights and disagreement.

Does **not** ship learned transformer probes; weights are **operator-set** or from
``adaptive_weights`` on toy calibration rows. See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from cos.sigma_gate import SigmaGate  # noqa: F401 — σ kernel anchor (sigma_gate.h / Python lite)

import statistics
from typing import Any, Dict, Mapping, Optional, Sequence, Tuple

__all__ = ["SigmaEnsemble"]

_DEFAULT_PROBES: Tuple[str, ...] = ("entropy", "hide", "icr", "lsd", "spectral", "sink", "energy", "sep")


class SigmaEnsemble:
    """Weighted mean of named probe scores + RETHINK when probes disagree."""

    def __init__(
        self,
        probes: Optional[Sequence[str]] = None,
        *,
        weights: Optional[Mapping[str, float]] = None,
    ) -> None:
        self.probe_names: Tuple[str, ...] = tuple(probes) if probes else _DEFAULT_PROBES
        self.weights: Dict[str, float] = {k: 1.0 for k in self.probe_names}
        if weights:
            for k, v in weights.items():
                if k in self.weights:
                    self.weights[str(k)] = float(v)
        self._renorm()

    def _renorm(self) -> None:
        s = sum(self.weights.values()) or 1.0
        self.weights = {k: float(v) / s for k, v in self.weights.items()}

    def score(
        self,
        probe_values: Mapping[str, float],
        *,
        hidden_states: Any = None,
        attention_maps: Any = None,
    ) -> Dict[str, Any]:
        """``probe_values`` maps channel name → scalar; extra args reserved for future wiring."""
        del hidden_states, attention_maps
        num = 0.0
        den = 0.0
        used: Dict[str, float] = {}
        for k in self.probe_names:
            if k in probe_values and probe_values[k] is not None:
                w = self.weights.get(k, 0.0)
                num += w * float(probe_values[k])
                den += w
                used[k] = float(probe_values[k])
        fused = float(num / den) if den > 0 else 0.5
        disc = self.disagreement_score(used)
        verdict = "RETHINK" if disc > 0.28 else "ACCEPT"
        return {"sigma": round(fused, 6), "verdict": verdict, "used": used, "disagreement": disc}

    def adaptive_weights(self, calibration_data: Sequence[Mapping[str, Any]]) -> Dict[str, float]:
        """Inverse-error weights from rows with ``target_sigma`` and per-probe errors (toy)."""
        err: Dict[str, float] = {k: 0.0 for k in self.probe_names}
        n = 0
        for row in calibration_data:
            target = float(row.get("target_sigma", 0.5))
            for k in self.probe_names:
                if k in row:
                    err[k] += abs(float(row[k]) - target)
            n += 1
        if n <= 0:
            return dict(self.weights)
        inv = {k: 1.0 / (1e-6 + err[k] / n) for k in self.probe_names}
        s = sum(inv.values()) or 1.0
        return {k: round(inv[k] / s, 6) for k in self.probe_names}

    @staticmethod
    def disagreement_score(probe_scores: Mapping[str, float]) -> float:
        vals = list(probe_scores.values())
        if len(vals) < 2:
            return 0.0
        return float(statistics.pstdev(vals))

    def leave_one_out(self, probe_values: Mapping[str, float]) -> Dict[str, Any]:
        """Fused sigma with each probe omitted; reports spread (stability check)."""
        keys = [k for k in self.probe_names if k in probe_values]
        if not keys:
            return {"spread": 0.0, "per_drop": {}}
        full = float(self.score(probe_values)["sigma"])
        per: Dict[str, float] = {}
        for drop in keys:
            sub = {k: v for k, v in probe_values.items() if k != drop}
            per[drop] = float(self.score(sub)["sigma"])
        spread = max(abs(full - v) for v in per.values()) if per else 0.0
        return {"spread": round(spread, 6), "baseline": full, "per_drop": per}

    def fallback(self, probe_values: Mapping[str, float]) -> Dict[str, Any]:
        """If any value is NaN-like (None / missing), use median of the rest."""
        clean: Dict[str, float] = {}
        for k, v in probe_values.items():
            if v is None:
                continue
            try:
                clean[k] = float(v)
            except (TypeError, ValueError):
                continue
        if not clean:
            return {"sigma": 0.5, "mode": "empty"}
        if len(clean) < len(probe_values):
            med = float(statistics.median(clean.values()))
            return {"sigma": med, "mode": "median_fallback", "used": clean}
        return {"sigma": float(self.score(clean)["sigma"]), "mode": "full", "used": clean}
