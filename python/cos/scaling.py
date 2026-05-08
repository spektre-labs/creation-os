# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-scaling law (lab): relate **compute budget** to **coherence scalar σ**.

Chinchilla-style scaling ties data and parameters to loss. Here we record an **empirical**
bridge σ(C) ≈ A·C^(−α) + E: how much extra compute correlates with lower σ on scored pairs.

**Scope:** curve fit over user-supplied (compute, σ) tuples from :class:`~cos.sigma_gate.SigmaGate`.
Not a theorem about all LMs; **E** is a fitted floor, not a formal Gödel bound.

**NOT AGI ACHIEVED** — see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

import math
from typing import Any, Dict, List, Optional

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaScaling"]


class SigmaScaling:
    """Fit and query a power-law-with-floor curve: σ(C) ≈ A·C^(−α) + E."""

    def __init__(self, gate: Any = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.measurements: List[Dict[str, Any]] = []

    def measure(self, compute_units: float, prompt: str, response: str) -> Dict[str, Any]:
        """Record one (compute, σ) point; ``compute_units`` must be positive for fitting."""
        σ_raw, verdict = self.gate.score(str(prompt), str(response))
        σ = float(σ_raw)
        self.measurements.append(
            {
                "compute": float(compute_units),
                "σ": σ,
                "verdict": str(verdict),
            }
        )
        return {"compute": float(compute_units), "σ": round(σ, 4)}

    def fit_law(self) -> Dict[str, Any]:
        """Fit log–linear model: log(σ − E) = log(A) − α·log(C)."""
        if len(self.measurements) < 3:
            return {"error": "need at least 3 measurements"}

        σ_values = [float(m["σ"]) for m in self.measurements]
        c_values = [float(m["compute"]) for m in self.measurements]

        if any(c <= 0.0 for c in c_values):
            return {"error": "compute must be positive"}

        σ_min = min(σ_values)
        E = max(1e-6, σ_min * 0.9)

        valid = [(c, σ) for c, σ in zip(c_values, σ_values) if σ > E]
        if len(valid) < 2:
            return {"error": "insufficient valid points (need sigma > E)"}

        log_c = [math.log(c) for c, _ in valid]
        log_res = [math.log(σ - E) for _, σ in valid]

        n = len(log_c)
        sum_x = sum(log_c)
        sum_y = sum(log_res)
        sum_xy = sum(x * y for x, y in zip(log_c, log_res))
        sum_xx = sum(x * x for x in log_c)

        denom = n * sum_xx - sum_x**2
        if abs(denom) < 1e-12:
            return {"error": "degenerate data (collinear log compute)"}

        slope = (n * sum_xy - sum_x * sum_y) / denom  # slope = -alpha
        alpha = -float(slope)
        log_A = (sum_y - slope * sum_x) / n
        A = float(math.exp(log_A))

        if alpha <= 1e-8:
            return {"error": "non-positive efficiency exponent (alpha)"}
        if A <= 0.0 or not math.isfinite(A):
            return {"error": "invalid scale A"}

        return {
            "A": round(A, 4),
            "α": round(alpha, 4),
            "E": round(E, 4),
            "law": f"σ(C) = {A:.4f} * C^(-{alpha:.4f}) + {E:.4f}",
            "n_points": len(valid),
        }

    def predict_σ(self, compute: float, law: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Predict σ at compute budget ``C`` using fitted ``law``."""
        fitted = law if law is not None else self.fit_law()
        if "error" in fitted:
            return {"error": fitted["error"]}

        if compute <= 0.0:
            return {"error": "compute must be positive"}

        A, alpha, E = float(fitted["A"]), float(fitted["α"]), float(fitted["E"])
        raw = A * float(compute) ** (-alpha) + E
        σ_predicted = max(E, min(1.0, raw))
        return {
            "compute": float(compute),
            "σ_predicted": round(σ_predicted, 4),
        }

    def compute_needed(self, target_σ: float, law: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """Invert the law: compute C with σ(C) ≈ target (ignores clamp above 1)."""
        fitted = law if law is not None else self.fit_law()
        if "error" in fitted:
            return {"error": fitted["error"]}

        A, alpha, E = float(fitted["A"]), float(fitted["α"]), float(fitted["E"])
        if target_σ <= E:
            return {
                "target_σ": float(target_σ),
                "compute_needed": float("inf"),
                "reason": f"target σ={target_σ} at or below floor E={E}",
            }

        inner = (target_σ - E) / A
        if inner <= 0.0:
            return {"error": "target below A-adjusted floor"}

        C = inner ** (-1.0 / alpha)
        if not math.isfinite(C) or C <= 0.0:
            return {"error": "invalid compute estimate"}

        return {
            "target_σ": float(target_σ),
            "compute_needed": round(float(C), 2),
        }

    def efficiency_frontier(self, n_points: int = 10) -> Dict[str, Any]:
        """Sample predicted σ over a compute grid spanning observed measurements."""
        law = self.fit_law()
        if "error" in law:
            return {"error": law["error"]}

        n_points = max(1, int(n_points))
        c_list = [float(m["compute"]) for m in self.measurements if float(m["compute"]) > 0.0]
        max_c = max(c_list) * 2.0
        min_c = max(min(c_list) * 0.5, 1e-9)
        step = (max_c - min_c) / n_points if max_c > min_c else 1.0

        frontier: List[Dict[str, Any]] = []
        for i in range(n_points + 1):
            c = min_c + i * step
            pred = self.predict_σ(c, law)
            if "error" not in pred:
                frontier.append(pred)

        alpha = float(law["α"])
        factor = 2.0 ** (-alpha) if alpha > 0 else float("nan")

        return {
            "frontier": frontier,
            "law": law,
            "insight": (
                f"σ excess shrinks ~{factor:.4f}× when compute doubles (A·C^(−α) term; same E). "
                f"Irreducible floor E≈{law['E']:.4f}."
            ),
        }
