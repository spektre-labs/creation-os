# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Quorum over swarm agent proposals (σ-threshold vote; lab)."""
from __future__ import annotations

from typing import List, Sequence

from cos.sigma_swarm_agent import SigmaSwarmAgent, SwarmTask


class SigmaQuorum:
    def reach_consensus(
        self,
        agents: Sequence[SigmaSwarmAgent],
        task: SwarmTask,
        *,
        min_agreement: int = 3,
    ) -> dict:
        rows: List[dict] = []
        for a in agents:
            rows.append(a.propose(task))
        calm = [r for r in rows if float(r.get("sigma", 1.0)) < 0.5]
        k = max(1, int(min_agreement))
        if len(calm) >= k:
            best = min(calm, key=lambda r: float(r["sigma"]))
            return {
                "consensus": True,
                "winner": best,
                "calm_agents": len(calm),
                "n_agents": len(rows),
            }
        return {
            "consensus": False,
            "n_agents": len(rows),
            "calm_agents": len(calm),
            "needed": k,
            "samples": rows,
        }


__all__ = ["SigmaQuorum"]
