# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Open-ended goal discovery over σ-shaped graph frontiers (lab).

Expands the *objective space* from uncertain triples (medium σ), not a fixed reward.
Complements :mod:`cos.meta_goal` and the Ω-loop in :mod:`cos.omega`.

**NOT AGI ACHIEVED** — intrinsic exploration hooks only; see ``docs/CLAIM_DISCIPLINE.md``."""
from __future__ import annotations

from typing import Any, Dict, List, Optional, Sequence

__all__ = ["SigmaOpenEnded"]


class SigmaOpenEnded:
    """Frontier scan + domain clustering + exploration goals (optional ``meta_goal`` hooks)."""

    def __init__(
        self,
        gate: Any = None,
        graph: Any = None,
        meta_goal: Any = None,
    ) -> None:
        from cos.sigma_gate import SigmaGate

        self.gate = gate or SigmaGate()
        self.graph = graph
        self.meta_goal = meta_goal
        self.discovered_domains: List[Dict[str, Any]] = []
        self.frontier: List[Dict[str, Any]] = []

    @staticmethod
    def _other_endpoint(entity: str, rel: Dict[str, Any]) -> str:
        subj = str(rel.get("subject", ""))
        obj = str(rel.get("object", ""))
        el = entity.lower()
        if subj.lower() == el:
            return obj
        if obj.lower() == el:
            return subj
        return obj

    def scan_frontier(self) -> List[Dict[str, Any]]:
        """Incident triples with σ in (0.3, 0.8) — uncertain but not hopeless."""
        if self.graph is None:
            return []
        frontier: List[Dict[str, Any]] = []
        try:
            entities = self.graph.entities()
        except Exception:
            return []

        for entity in entities:
            try:
                rels = self.graph.relations_of(entity)
            except Exception:
                continue
            for rel in rels:
                σ = float(rel.get("sigma", 0.5))
                if 0.3 < σ < 0.8:
                    frontier.append(
                        {
                            "entity": entity,
                            "relation": rel.get("relation"),
                            "target": rel.get("object"),
                            "σ": round(σ, 4),
                            "potential": round(1.0 - σ, 4),
                        }
                    )

        frontier.sort(key=lambda x: x["potential"], reverse=True)
        self.frontier = frontier[:20]
        return self.frontier

    def discover_domain(self, seed_entities: Sequence[str]) -> Optional[Dict[str, Any]]:
        """BFS outward from seeds along lower-σ edges (σ < 0.6)."""
        if self.graph is None:
            return None

        explored: set[str] = set()
        domain_entities: set[str] = {str(s) for s in seed_entities}
        queue: List[str] = [str(s) for s in seed_entities]
        domain_σ_sum = 0.0
        domain_count = 0

        while queue and len(domain_entities) < 50:
            entity = queue.pop(0)
            if entity in explored:
                continue
            explored.add(entity)

            try:
                rels = self.graph.relations_of(entity)
            except Exception:
                continue
            for rel in rels:
                target = self._other_endpoint(entity, rel)
                σ = float(rel.get("sigma", 0.5))
                if target not in domain_entities and σ < 0.6:
                    domain_entities.add(target)
                    queue.append(target)
                    domain_σ_sum += σ
                    domain_count += 1

        domain = {
            "entities": sorted(domain_entities, key=lambda x: x.lower()),
            "size": len(domain_entities),
            "avg_σ": round(domain_σ_sum / max(domain_count, 1), 4),
            "seed": list(seed_entities),
        }
        self.discovered_domains.append(domain)
        return domain

    def generate_exploration_goals(self, n: int = 5) -> List[Dict[str, Any]]:
        frontier = self.scan_frontier()
        goals: List[Dict[str, Any]] = []
        for item in frontier[: max(0, int(n))]:
            ent = item.get("entity")
            rel = item.get("relation")
            goal = {
                "type": "explore",
                "target": ent,
                "question": (
                    f"Learn more about {ent} and its {rel} relationships"
                    if ent is not None and rel is not None
                    else f"Learn more about frontier item {item}"
                ),
                "current_σ": item["σ"],
                "expected_value": item["potential"],
            }
            goals.append(goal)
            if self.meta_goal is not None and hasattr(self.meta_goal, "register_skill"):
                try:
                    self.meta_goal.register_skill(f"explore_{ent}")
                except Exception:
                    pass
        return goals

    def novelty_score(self, observation: Any) -> float:
        if self.graph is None:
            return 0.5
        try:
            known = self.graph.entities()
        except Exception:
            return 0.5
        words = [w for w in str(observation).lower().split() if w]
        if not words:
            return 1.0
        known_lower = [e.lower() for e in known]
        overlap = 0
        for w in words:
            if any(w == kl or w in kl or kl in w for kl in known_lower):
                overlap += 1
        novelty = 1.0 - (overlap / len(words))
        return round(max(0.0, min(1.0, novelty)), 4)

    def should_explore_or_exploit(self) -> str:
        if not self.frontier:
            self.scan_frontier()
        if not self.frontier:
            return "explore"
        avg_frontier_σ = sum(float(f["σ"]) for f in self.frontier) / len(self.frontier)
        if avg_frontier_σ > 0.5:
            return "explore"
        return "exploit"
