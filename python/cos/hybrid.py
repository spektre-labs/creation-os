# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-hybrid — edge-first routing with optional cloud escalation (lab).

Privacy: operator must enforce PII policy; helpers only encode intent."""
from __future__ import annotations

from typing import Any, Callable, Dict, Optional, Sequence

from cos.sigma_gate import ABSTAIN, ACCEPT

__all__ = ["SigmaHybrid"]

GenerateFn = Callable[[str], str]


class SigmaHybrid:
    """Route: edge → σ → ACCEPT return; RETHINK → cloud; ABSTAIN → withhold (no cloud)."""

    def __init__(self, gate: Any = None) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self._prefetch: Dict[str, str] = {}

    def route(
        self,
        query: str,
        edge_model: Any,
        cloud_model: Any,
        gate: Optional[Any] = None,
    ) -> Dict[str, Any]:
        g = gate or self.gate
        q = str(query)
        edge_fn = self._gen_fn(edge_model)
        cloud_fn = self._gen_fn(cloud_model)
        if q in self._prefetch:
            edge_text = self._prefetch[q]
        else:
            edge_text = edge_fn(q)
        s_e, v_e = g.score(q, edge_text)
        out: Dict[str, Any] = {
            "query": q,
            "edge_sigma": float(s_e),
            "edge_verdict": str(v_e),
            "edge_response": edge_text,
            "source": None,
        }
        if str(v_e).upper() == ACCEPT:
            out["source"] = "edge"
            out["final_response"] = edge_text
            out["sigma"] = float(s_e)
            out["verdict"] = str(v_e)
            return out
        if str(v_e).upper() == ABSTAIN:
            out["source"] = "abstain"
            out["final_response"] = "I don't know."
            out["sigma"] = float(s_e)
            out["verdict"] = str(v_e)
            return out
        cloud_text = cloud_fn(q)
        s_c, v_c = g.score(q, cloud_text)
        out["cloud_sigma"] = float(s_c)
        out["cloud_verdict"] = str(v_c)
        out["cloud_response"] = cloud_text
        out["source"] = "cloud"
        out["final_response"] = cloud_text
        out["sigma"] = float(s_c)
        out["verdict"] = str(v_c)
        return out

    @staticmethod
    def _gen_fn(model: Any) -> GenerateFn:
        if callable(model) and not hasattr(model, "generate"):
            return lambda q: str(model(q))
        gen = getattr(model, "generate", None)
        if callable(gen):
            return lambda q: str(gen(q))
        return lambda _q: str(model)

    @staticmethod
    def adaptive_threshold(
        latency_ms_edge: float,
        battery_percent: float,
        network_quality_0_1: float,
        *,
        base_tau_accept: float = 0.3,
        base_tau_abstain: float = 0.7,
    ) -> Dict[str, Any]:
        """Raise thresholds (stricter edge) when network poor or battery low — lab policy."""
        nq = max(0.0, min(1.0, float(network_quality_0_1)))
        bat = max(0.0, min(100.0, float(battery_percent)))
        lat = max(0.0, float(latency_ms_edge))
        bump = (1.0 - nq) * 0.08 + max(0.0, 20.0 - bat) / 100.0 * 0.06 + min(1.0, lat / 500.0) * 0.04
        ta = min(0.45, float(base_tau_accept) + bump)
        tb = min(0.95, float(base_tau_abstain) + bump)
        return {
            "tau_accept": round(ta, 4),
            "tau_abstain": round(tb, 4),
            "use_cloud_more_often": nq > 0.7 and bat > 40,
            "note": "Apply via SigmaGate assignments or persona profile — not automatic here.",
        }

    def prefetch(self, likely_queries: Sequence[str], edge_model: Any) -> Dict[str, Any]:
        edge_fn = self._gen_fn(edge_model)
        n = 0
        for q in likely_queries:
            qs = str(q).strip()
            if not qs:
                continue
            self._prefetch[qs] = edge_fn(qs)
            n += 1
        return {"prefetched": n}

    @staticmethod
    def privacy_routing(query: str, pii_detected: bool) -> Dict[str, Any]:
        del query
        if pii_detected:
            return {"allow_cloud": False, "route": "edge_only", "redact_before_log": True}
        return {"allow_cloud": True, "route": "hybrid", "redact_before_log": False}

    @staticmethod
    def cost_comparison(edge_cost: float, cloud_cost: float) -> Dict[str, Any]:
        ee, cc = float(edge_cost), float(cloud_cost)
        saved = max(0.0, cc - ee) if cc >= ee else 0.0
        return {
            "edge_cost": ee,
            "cloud_cost": cc,
            "savings_when_edge_chosen": round(saved, 6),
            "disclaimer": "Unit costs are operator-supplied — not market quotes.",
        }
