# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Open-ended discovery lab: graph frontiers (:class:`SigmaOpenEnded`) + MAP-Elites-style archives.

:class:`SigmaOpenEnded` expands objective space from uncertain triples on a graph.
:class:`DiscoveryArchive` is a small σ-scored **quality--diversity** toy (lower σ = higher nominal
quality in this lab). **Not** a proof that σ avoids Goodhart effects or that search never stalls —
see ``docs/CLAIM_DISCIPLINE.md``. **NOT AGI ACHIEVED**."""
from __future__ import annotations

from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

from cos.sigma_gate import SigmaGate

__all__ = ["DiscoveryArchive", "SigmaOpenEnded"]


class SigmaOpenEnded:
    """Frontier scan + domain clustering + exploration goals (optional ``meta_goal`` hooks)."""

    def __init__(
        self,
        gate: Any = None,
        graph: Any = None,
        meta_goal: Any = None,
    ) -> None:
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


def _fnv1a_32(data: bytes) -> int:
    h = 2166136261
    for b in data:
        h ^= b
        h = (h * 16777619) & 0xFFFFFFFF
    return h


GenerateFn = Callable[[int, Dict[str, Any]], Tuple[Any, Any]]


class DiscoveryArchive:
    """MAP-Elites-style **toy** archive: one cell per behavior bucket, σ as nominal quality."""

    def __init__(self, gate: Optional[Any] = None, grid_size: int = 10) -> None:
        self.gate = gate if gate is not None else SigmaGate()
        self.grid_size = max(1, int(grid_size))
        self.archive: Dict[Tuple[int, int], Dict[str, Any]] = {}
        self.total_discoveries = 0
        self.frontier_history: List[int] = []

    @staticmethod
    def _behavior_to_cell(behavior: Any, grid_size: int) -> Tuple[int, int]:
        raw = str(behavior).encode("utf-8", errors="replace")
        h = _fnv1a_32(raw)
        x = int(h % grid_size)
        y = int((h // grid_size) % grid_size)
        return (x, y)

    def _cell(self, behavior: Any) -> Tuple[int, int]:
        return self._behavior_to_cell(behavior, self.grid_size)

    def attempt(
        self,
        solution: Any,
        behavior_descriptor: Any,
        context: str = "",
    ) -> Dict[str, Any]:
        cell = self._cell(behavior_descriptor)
        σ, _verdict = self.gate.score(context or "quality", str(solution))
        σ = float(σ)

        novelty = self._compute_novelty(behavior_descriptor)
        interestingness = novelty * 0.5 + σ * 0.5

        result: Dict[str, Any] = {
            "cell": cell,
            "σ": round(σ, 4),
            "novelty": round(novelty, 4),
            "interestingness": round(interestingness, 4),
            "accepted": False,
        }

        if cell not in self.archive:
            self.archive[cell] = {
                "solution": solution,
                "σ": σ,
                "novelty": novelty,
                "behavior": behavior_descriptor,
            }
            result["accepted"] = True
            result["reason"] = "new cell discovered"
            self.total_discoveries += 1
        elif σ < self.archive[cell]["σ"]:
            improvement = float(self.archive[cell]["σ"]) - σ
            self.archive[cell] = {
                "solution": solution,
                "σ": σ,
                "novelty": novelty,
                "behavior": behavior_descriptor,
            }
            result["accepted"] = True
            result["reason"] = f"improved cell by Δσ={improvement:.3f}"
        else:
            result["reason"] = "existing solution is better"

        return result

    def _compute_novelty(self, behavior: Any) -> float:
        if not self.archive:
            return 1.0
        cell = self._cell(behavior)
        nearby = 0
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                if (cell[0] + dx, cell[1] + dy) in self.archive:
                    nearby += 1
        max_nearby = 25
        return round(max(0.0, min(1.0, 1.0 - nearby / float(max_nearby))), 4)

    def frontier(self) -> Dict[str, Any]:
        all_cells = {(x, y) for x in range(self.grid_size) for y in range(self.grid_size)}
        empty = all_cells - set(self.archive.keys())

        frontier: List[Dict[str, Any]] = []
        for cell in empty:
            nearby_σ: List[float] = []
            for dx in range(-1, 2):
                for dy in range(-1, 2):
                    neighbor = (cell[0] + dx, cell[1] + dy)
                    if neighbor in self.archive:
                        nearby_σ.append(float(self.archive[neighbor]["σ"]))

            if nearby_σ:
                avg_neighbor_σ = sum(nearby_σ) / float(len(nearby_σ))
                frontier.append(
                    {
                        "cell": cell,
                        "neighbor_avg_σ": round(avg_neighbor_σ, 4),
                        "n_neighbors": len(nearby_σ),
                    }
                )

        frontier.sort(key=lambda f: f["neighbor_avg_σ"])
        self.frontier_history.append(len(frontier))
        return {
            "frontier_cells": frontier[:10],
            "total_empty": len(empty),
            "coverage": round(len(self.archive) / float(self.grid_size**2), 4),
        }

    def explore(
        self,
        generate_fn: GenerateFn,
        n_attempts: int = 100,
        context: str = "",
    ) -> Dict[str, Any]:
        results: Dict[str, Any] = {
            "accepted": 0,
            "rejected": 0,
            "history": [],
        }
        for i in range(int(n_attempts)):
            solution, behavior = generate_fn(i, self.frontier())
            attempt_res = self.attempt(solution, behavior, context)
            if attempt_res["accepted"]:
                results["accepted"] += 1
            else:
                results["rejected"] += 1
            if i % 10 == 0:
                results["history"].append(
                    {
                        "step": i,
                        "coverage": round(len(self.archive) / float(self.grid_size**2), 4),
                        "discoveries": self.total_discoveries,
                    }
                )

        results["final_coverage"] = round(len(self.archive) / float(self.grid_size**2), 4)
        results["total_discoveries"] = self.total_discoveries
        return results

    def quality_diversity_score(self) -> float:
        if not self.archive:
            return 0.0
        return round(sum(1.0 - float(e["σ"]) for e in self.archive.values()), 2)

    def status(self) -> Dict[str, Any]:
        n = len(self.archive)
        return {
            "archive_size": n,
            "grid_size": self.grid_size,
            "coverage": round(n / float(self.grid_size**2), 4),
            "qd_score": self.quality_diversity_score(),
            "total_discoveries": self.total_discoveries,
            "avg_σ": round(
                sum(float(e["σ"]) for e in self.archive.values()) / float(max(n, 1)),
                4,
            )
            if self.archive
            else 0.5,
        }
