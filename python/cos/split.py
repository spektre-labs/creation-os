# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-split — edge vs cloud routing from σ (lab partition helpers).

:class:`SigmaSplit` keeps lightweight placement math. :class:`SplitRouter` and
:class:`FleetManager` add σ-gated pre-checks, optional edge→cloud fallback, and a
toy heterogeneous-device picker (no real cluster / Splitwise claims — see
``docs/CLAIM_DISCIPLINE.md``).

``bandwidth_mbps`` nudges :class:`SigmaSplit` threshold; ``privacy_sigma`` caps what is marked
elevated-risk for offload. No real device graphs."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["SigmaSplit", "SplitRouter", "FleetManager"]


class SigmaSplit:
    """Route workloads by σ; simple forward stubs for edge/cloud halves."""

    def __init__(self, *, sigma_edge_threshold: float = 0.4) -> None:
        self.sigma_edge_threshold = float(sigma_edge_threshold)

    def route(
        self,
        sigma: float,
        *,
        bandwidth_mbps: Optional[float] = None,
    ) -> Dict[str, Any]:
        th = self.sigma_edge_threshold
        if bandwidth_mbps is not None and float(bandwidth_mbps) < 20.0:
            th = min(0.85, th + 0.08)
        sg = float(sigma)
        placement = "edge" if sg <= th else "cloud"
        return {
            "placement": placement,
            "sigma_threshold_used": round(th, 4),
            "sigma": sg,
        }

    def partition(
        self,
        n_layers: int,
        sigma_threshold: float,
    ) -> Dict[str, List[int]]:
        n = max(1, int(n_layers))
        cut = int(round(n * float(sigma_threshold)))
        cut = max(0, min(n, cut))
        return {"edge_layers": list(range(cut)), "cloud_layers": list(range(cut, n))}

    def edge_forward(self, x: Any, edge_model: Any) -> Dict[str, Any]:
        fn: Callable[[Any], Any] = edge_model if callable(edge_model) else (lambda z: z)
        y = fn(x)
        return {"output": y, "stage": "edge"}

    def cloud_forward(self, x: Any, cloud_model: Any) -> Dict[str, Any]:
        fn: Callable[[Any], Any] = cloud_model if callable(cloud_model) else (lambda z: z)
        y = fn(x)
        return {"output": y, "stage": "cloud"}

    def privacy_mask(self, sigma_local: float) -> Dict[str, Any]:
        """High σ local slice → mark as do-not-send (lab flag)."""
        return {
            "send_allowed": float(sigma_local) < 0.75,
            "sigma": float(sigma_local),
        }


RouteFn = Callable[[str], str]


class SplitRouter:
    """Pre-score prompts with σ; low-σ first hop on edge, high-σ on cloud (lab)."""

    _PRECHECK_PROMPT = "difficulty assessment"

    def __init__(self, gate: Optional[Any] = None, edge_threshold: float = 0.3) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.edge_threshold = float(edge_threshold)
        self.stats: Dict[str, int] = {"edge": 0, "cloud": 0, "total": 0}

    def route(
        self,
        prompt: str,
        edge_fn: Optional[RouteFn] = None,
        cloud_fn: Optional[RouteFn] = None,
    ) -> Dict[str, Any]:
        """σ pre-check on ``(difficulty assessment, prompt)``; optional edge ABSTAIN → cloud retry."""
        self.stats["total"] += 1
        sigma_pre, _v0 = self.gate.score(self._PRECHECK_PROMPT, str(prompt))

        if float(sigma_pre) < self.edge_threshold:
            self.stats["edge"] += 1
            if edge_fn is not None:
                response = edge_fn(str(prompt))
            else:
                response = f"[EDGE] processed: {str(prompt)[:50]}"

            sigma_post, verdict = self.gate.score(str(prompt), str(response))
            verdict_s = str(verdict).split(".")[-1]

            if verdict_s == "ABSTAIN" and cloud_fn is not None:
                self.stats["cloud"] += 1
                response = cloud_fn(str(prompt))
                sigma_post, verdict = self.gate.score(str(prompt), str(response))
                verdict_s = str(verdict).split(".")[-1]
                return {
                    "source": "cloud_fallback",
                    "response": response,
                    "sigma_pre": round(float(sigma_pre), 4),
                    "sigma_post": round(float(sigma_post), 4),
                    "verdict": verdict_s,
                }

            return {
                "source": "edge",
                "response": response,
                "sigma_pre": round(float(sigma_pre), 4),
                "sigma_post": round(float(sigma_post), 4),
                "verdict": verdict_s,
            }

        self.stats["cloud"] += 1
        if cloud_fn is not None:
            response = cloud_fn(str(prompt))
        else:
            response = f"[CLOUD] processed: {str(prompt)[:50]}"

        sigma_post, verdict = self.gate.score(str(prompt), str(response))
        verdict_s = str(verdict).split(".")[-1]
        return {
            "source": "cloud",
            "response": response,
            "sigma_pre": round(float(sigma_pre), 4),
            "sigma_post": round(float(sigma_post), 4),
            "verdict": verdict_s,
        }

    def edge_ratio(self) -> float:
        """Share of requests that took the edge-first branch (may still cloud-fallback)."""
        if self.stats["total"] == 0:
            return 0.0
        return round(self.stats["edge"] / float(self.stats["total"]), 4)

    def cost_savings(self, cloud_cost_per_call: float = 0.01) -> Dict[str, Any]:
        """Toy savings: each edge-first hop avoids paying ``cloud_cost_per_call`` (lab bookkeeping)."""
        avoided = int(self.stats["edge"])
        cpc = float(cloud_cost_per_call)
        return {
            "cloud_calls_avoided": avoided,
            "estimated_savings": round(avoided * cpc, 4),
            "edge_ratio": self.edge_ratio(),
        }


class FleetManager:
    """Pick the lexicographically-cheapest *capability* tier that can absorb ``σ_pre``."""

    _CAP_ORDER: Dict[str, int] = {"tiny": 0, "edge": 1, "desktop": 2, "server": 3}
    _ROUTE_PROBE = "route"

    def __init__(self, gate: Optional[Any] = None) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.devices: Dict[str, Dict[str, Any]] = {}

    def register(self, device_id: str, capability: str, max_sigma: float) -> None:
        self.devices[str(device_id)] = {
            "capability": str(capability),
            "max_sigma": float(max_sigma),
            "jobs_completed": 0,
            "avg_sigma": 0.5,
        }

    def assign(self, prompt: str) -> Dict[str, Any]:
        sigma_pre, _v = self.gate.score(self._ROUTE_PROBE, str(prompt))
        sg = float(sigma_pre)

        candidates: List[Tuple[str, Dict[str, Any]]] = [
            (d_id, d) for d_id, d in self.devices.items() if sg <= float(d["max_sigma"])
        ]
        if not candidates:
            return {"error": "no device capable", "sigma": round(sg, 4)}

        candidates.sort(key=lambda x: self._CAP_ORDER.get(str(x[1]["capability"]), 99))
        chosen_id, chosen = candidates[0]
        jobs = int(chosen["jobs_completed"]) + 1
        prev_avg = float(chosen["avg_sigma"])
        prev_n = int(chosen["jobs_completed"])
        chosen["avg_sigma"] = round((prev_avg * prev_n + sg) / float(jobs), 4)
        chosen["jobs_completed"] = jobs

        return {
            "device": chosen_id,
            "capability": chosen["capability"],
            "sigma_pre": round(sg, 4),
        }
