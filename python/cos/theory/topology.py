# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""**Topological** summaries of σ **traces** (lab toy, not full persistent homology).

Real **topological data analysis** builds filtrations and computes rigorously defined persistence
modules. This module uses **adaptive threshold crossings** on a 1D sequence, **run counting** for
a toy **β₀-style** statistic, a **lifetime multiset** summary, and an **L1 mismatch** on sorted
lifetimes — all **illustrative** hooks for **persistent vs ephemeral** language. It is **not** a
certified TDA pipeline, **not** a stability guarantee for σ, and **not** a substitute for standard
packages (**zero** extra dependencies). **Not AGI achieved.** See ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaTopology"]


class SigmaTopology:
    """1D σ-trace summaries; :class:`~cos.sigma_gate.SigmaGate` is optional (reserved for hooks)."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate or SigmaGate()

    def persistence_diagram(self, sigma_trace: Sequence[float]) -> Dict[str, Any]:
        """Birth–death **pairs** from crossings of the trace mean (sketch, not Vietoris–Rips)."""
        trace = [float(x) for x in sigma_trace]
        n = len(trace)
        if n < 3:
            return {"pairs": [], "n_features": 0, "persistent_features": 0, "noise_features": 0}

        pairs: List[Dict[str, Any]] = []
        birth: Optional[Dict[str, Any]] = None
        threshold = sum(trace) / n

        for i, s in enumerate(trace):
            if s < threshold and birth is None:
                birth = {"index": i, "sigma": s, "σ": s}
            elif s >= threshold and birth is not None:
                death = {"index": i, "sigma": s, "σ": s}
                lifetime = i - birth["index"]
                persistent = lifetime > n * 0.1
                pairs.append(
                    {
                        "birth": birth["index"],
                        "death": death["index"],
                        "lifetime": lifetime,
                        "birth_σ": round(float(birth["sigma"]), 4),
                        "death_σ": round(float(death["sigma"]), 4),
                        "birth_sigma": round(float(birth["sigma"]), 4),
                        "death_sigma": round(float(death["sigma"]), 4),
                        "persistent": persistent,
                    }
                )
                birth = None

        if birth is not None:
            pairs.append(
                {
                    "birth": birth["index"],
                    "death": n,
                    "lifetime": n - birth["index"],
                    "birth_σ": round(float(birth["sigma"]), 4),
                    "death_σ": None,
                    "birth_sigma": round(float(birth["sigma"]), 4),
                    "death_sigma": None,
                    "persistent": True,
                }
            )

        persistent_n = sum(1 for p in pairs if p["persistent"])
        return {
            "pairs": pairs,
            "n_features": len(pairs),
            "persistent_features": persistent_n,
            "noise_features": len(pairs) - persistent_n,
        }

    def betti_numbers(
        self,
        sigma_trace: Sequence[float],
        thresholds: Optional[Sequence[float]] = None,
    ) -> List[Dict[str, Any]]:
        """Toy **β₀**: count runs of indices with σ strictly below ``t``."""
        trace = [float(x) for x in sigma_trace]
        th = list(thresholds) if thresholds is not None else [i / 10.0 for i in range(11)]

        betti: List[Dict[str, Any]] = []
        for t in th:
            regions = 0
            in_region = False
            for s in trace:
                if s < t and not in_region:
                    regions += 1
                    in_region = True
                elif s >= t:
                    in_region = False
            betti.append(
                {
                    "threshold": float(t),
                    "β0": regions,
                    "beta0": regions,
                }
            )
        return betti

    def landscape(self, sigma_trace: Sequence[float], k: int = 3) -> Dict[str, Any]:
        """Order statistics of persistence lifetimes (not a functional persistence landscape)."""
        diagram = self.persistence_diagram(sigma_trace)
        trace = [float(x) for x in sigma_trace]
        lifetimes = sorted((int(p["lifetime"]) for p in diagram["pairs"]), reverse=True)

        landscapes: List[Dict[str, Any]] = []
        cap = min(int(k), len(lifetimes))
        denom = max(len(trace), 1)
        for i in range(cap):
            lt = lifetimes[i]
            landscapes.append(
                {
                    "k": i + 1,
                    "lifetime": lt,
                    "dominance": round(float(lt) / float(denom), 4),
                }
            )

        return {
            "landscapes": landscapes,
            "dominant_feature_lifetime": lifetimes[0] if lifetimes else 0,
            "topological_complexity": len(lifetimes),
        }

    def sigma_manifold_curvature(
        self,
        sigma_trace: Sequence[float],
        window: int = 5,
    ) -> Dict[str, Any]:
        """Discrete |second difference| along the trace as a rough smoothness proxy."""
        trace = [float(x) for x in sigma_trace]
        w = int(window)
        if len(trace) < w + 2:
            return {
                "avg_curvature": 0.0,
                "max_curvature": 0.0,
                "smooth": True,
                "interpretation": "insufficient samples",
            }

        curvatures: List[float] = []
        for i in range(1, len(trace) - 1):
            d2 = trace[i + 1] - 2.0 * trace[i] + trace[i - 1]
            curvatures.append(abs(d2))

        avg_curvature = sum(curvatures) / len(curvatures)
        max_curvature = max(curvatures)
        smooth = avg_curvature < 0.01
        if avg_curvature < 0.01:
            interpretation = "geodesic (smooth recovery)"
        elif avg_curvature > 0.05:
            interpretation = "turbulent (rapid changes)"
        else:
            interpretation = "moderate dynamics"

        return {
            "avg_curvature": round(avg_curvature, 6),
            "max_curvature": round(max_curvature, 6),
            "smooth": smooth,
            "interpretation": interpretation,
        }

    def σ_manifold_curvature(  # noqa: PLC2401 — σ API spelling
        self,
        sigma_trace: Sequence[float],
        window: int = 5,
    ) -> Dict[str, Any]:
        return self.sigma_manifold_curvature(sigma_trace, window)

    def wasserstein_distance(
        self,
        trace_a: Sequence[float],
        trace_b: Sequence[float],
    ) -> float:
        """Sorted **L1** mismatch of lifetime multisets (proxy label, not true Wasserstein–∞)."""
        diag_a = self.persistence_diagram(trace_a)
        diag_b = self.persistence_diagram(trace_b)

        lifetimes_a = sorted(int(p["lifetime"]) for p in diag_a["pairs"])
        lifetimes_b = sorted(int(p["lifetime"]) for p in diag_b["pairs"])

        max_len = max(len(lifetimes_a), len(lifetimes_b))
        lifetimes_a = list(lifetimes_a) + [0] * (max_len - len(lifetimes_a))
        lifetimes_b = list(lifetimes_b) + [0] * (max_len - len(lifetimes_b))

        distance = sum(abs(a - b) for a, b in zip(lifetimes_a, lifetimes_b))
        return round(float(distance) / float(max(max_len, 1)), 4)
