# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-drift — baseline σ distribution vs current (KL / W1-style proxy, lab).

Does not replace production observability; wire real histograms from ``cos.observe``.
See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
import statistics
import time
from typing import Any, Dict, List, Mapping, Sequence

__all__ = ["SigmaDrift"]


def _histogram(xs: Sequence[float], *, bins: int = 8) -> List[float]:
    if not xs:
        return [1.0 / bins] * bins
    lo, hi = min(xs), max(xs)
    if lo == hi:
        h = [0.0] * bins
        h[bins // 2] = 1.0
        return h
    w = (hi - lo) / bins
    h = [0.0] * bins
    for x in xs:
        i = min(bins - 1, max(0, int((float(x) - lo) / w) if w > 0 else 0))
        h[i] += 1.0
    s = sum(h) or 1.0
    return [c / s for c in h]


def _kl_divergence(p: Sequence[float], q: Sequence[float], eps: float = 1e-9) -> float:
    return sum(float(pi) * math.log((float(pi) + eps) / (float(qi) + eps)) for pi, qi in zip(p, q))


def _wasserstein_1d(a: Sequence[float], b: Sequence[float]) -> float:
    aa, bb = sorted(a), sorted(b)
    if not aa or not bb:
        return 0.5
    # pad shorter with median
    while len(aa) < len(bb):
        aa.append(aa[-1])
    while len(bb) < len(aa):
        bb.append(bb[-1])
    return float(sum(abs(x - y) for x, y in zip(aa, bb)) / len(aa))


class SigmaDrift:
    """Store a reference σ list; compare incoming batches."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._baseline_sigmas: List[float] = []
        self._history: List[Dict[str, Any]] = []

    def baseline(self, gate: Any, reference_dataset: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        g = gate
        xs = []
        for row in reference_dataset:
            s, _ = g.score(str(row.get("prompt", "")), str(row.get("response", "")))
            xs.append(float(s))
        self._baseline_sigmas = xs
        return {"n": len(xs), "mean_sigma": round(float(statistics.mean(xs)), 6) if xs else 0.0}

    def detect(self, gate: Any, current_data: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        if not self._baseline_sigmas:
            return {"error": "no_baseline", "drift_score": 1.0}
        g = gate
        cur = []
        for row in current_data:
            s, _ = g.score(str(row.get("prompt", "")), str(row.get("response", "")))
            cur.append(float(s))
        h0 = _histogram(self._baseline_sigmas)
        h1 = _histogram(cur)
        kl = _kl_divergence(h0, h1) + _kl_divergence(h1, h0)
        w1 = _wasserstein_1d(self._baseline_sigmas, cur)
        drift = min(1.0, 0.45 * kl + 0.55 * min(1.0, w1))
        detail = {"kl_sym": round(kl, 6), "w1_proxy": round(w1, 6)}
        self._history.append({"t": time.time(), "drift_score": drift, **detail})
        return {"drift_score": round(drift, 6), **detail, "histogram_shift": "see_kl"}

    @staticmethod
    def alert_threshold(detect_result: Mapping[str, Any], *, limit: float = 0.35) -> Dict[str, Any]:
        ds = float(detect_result.get("drift_score", 0.0))
        return {"alert": ds > float(limit), "drift_score": ds, "threshold": float(limit)}

    def auto_recalibrate(self, gate: Any, new_data: Sequence[Mapping[str, Any]]) -> Dict[str, Any]:
        """Nudge τ toward batch mean σ (toy); operators replace with real calibration."""
        g = gate
        xs = []
        for row in new_data:
            s, _ = g.score(str(row.get("prompt", "")), str(row.get("response", "")))
            xs.append(float(s))
        mu = float(statistics.mean(xs)) if xs else 0.5
        ta = float(getattr(g, "tau_accept", getattr(g, "threshold_accept", 0.35)))
        tb = float(getattr(g, "tau_abstain", getattr(g, "threshold_abstain", 0.75)))
        g.tau_accept = max(0.05, min(0.9, ta + 0.5 * (mu - ta)))
        g.tau_abstain = max(g.tau_accept + 0.05, min(0.95, tb + 0.5 * (mu - tb)))
        return {"tau_accept": round(float(g.tau_accept), 6), "tau_abstain": round(float(g.tau_abstain), 6)}

    @staticmethod
    def root_cause(drift_details: Mapping[str, Any]) -> Dict[str, Any]:
        kl = float(drift_details.get("kl_sym", 0.0))
        w1 = float(drift_details.get("w1_proxy", 0.0))
        causes = []
        if kl > 0.2:
            causes.append("distribution_shape_shift")
        if w1 > 0.25:
            causes.append("central_tendency_shift")
        if not causes:
            causes.append("within_noise_or_insufficient_signal")
        return {"likely_causes": causes, "next_steps": "Check model version, data mix, user cohort."}

    def temporal_trend(self, window_days: float) -> Dict[str, Any]:
        """Summarize recorded ``detect`` calls in the last ``window_days``."""
        cutoff = time.time() - float(window_days) * 86400.0
        recent = [h for h in self._history if h.get("t", 0) >= cutoff]
        if not recent:
            return {"n": 0, "mean_drift": 0.0}
        m = statistics.mean(float(h["drift_score"]) for h in recent)
        return {"n": len(recent), "mean_drift": round(float(m), 6), "window_days": window_days}
