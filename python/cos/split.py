# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""σ-split — edge vs cloud routing from σ (lab partition helpers).

``bandwidth_mbps`` nudges the threshold; ``privacy_sigma`` caps what is marked
elevated-risk for offload. No real device graphs; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional

__all__ = ["SigmaSplit"]


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
