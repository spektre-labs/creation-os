# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""Dream cycle — σ-validated knowledge graph maintenance (:class:`DreamCycle`).

Each inferred or deduplicated change is scored with :class:`~cos.sigma_gate.SigmaGate`.
**ABSTAIN** inference proposals are never written. No mandatory third-party deps.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Set

from cos.graph import SigmaGraph

__all__ = ["DreamCycle", "run_dream_maintenance", "sigma_after_maintenance"]


def _verdict_str(verdict: object) -> str:
    if hasattr(verdict, "name"):
        return str(getattr(verdict, "name"))
    raw = str(verdict)
    if "." in raw:
        return raw.rsplit(".", 1)[-1]
    return raw


class DreamCycle:
    """Knowledge graph maintenance with σ-validation (dream cycle)."""

    def __init__(self, graph: SigmaGraph, gate: Any = None) -> None:
        self.graph = graph
        self.gate = gate if gate is not None else graph.gate
        self.report: Dict[str, Any] = {
            "deduped": 0,
            "decayed": 0,
            "inferred": 0,
            "orphans": 0,
            "insights": [],
        }

    def run(
        self,
        *,
        dedup_threshold: float = 0.93,
        max_age_days: float = 30.0,
        max_inferences: int = 20,
        insight_top_n: int = 5,
    ) -> Dict[str, Any]:
        """Run dedup → decay → infer → orphan cleanup → insights."""
        self.report = {
            "deduped": 0,
            "decayed": 0,
            "inferred": 0,
            "orphans": 0,
            "insights": [],
        }
        self.deduplicate(threshold=dedup_threshold)
        self.decay(max_age_days=max_age_days)
        self.infer_relations(max_inferences=max_inferences)
        self.cleanup_orphans()
        self.generate_insights(top_n=insight_top_n)
        return self.report

    def deduplicate(self, threshold: float = 0.93) -> None:
        """Merge similar entity labels (Jaccard over word tokens)."""
        cut = float(threshold)
        ents = self.graph.entities()
        merged: Set[str] = set()
        for i, a in enumerate(ents):
            if a.lower() in merged:
                continue
            for b in ents[i + 1 :]:
                if b.lower() in merged:
                    continue
                if self._similarity(a, b) >= cut:
                    self.graph.merge_entities(a, b)
                    merged.add(b.lower())
                    self.report["deduped"] += 1

    def decay(self, max_age_days: float = 30.0) -> None:
        """Raise σ for stale triples (epistemic drift); very high σ drops edges."""
        now = time.time()
        for row in self.graph.all_triples():
            tid = str(row["id"])
            created = float(row.get("created", now))
            age_days = (now - created) / 86400.0
            if age_days > max_age_days:
                extra = min(0.3, (age_days - max_age_days) / 100.0)
                new_sigma = min(1.0, float(row["sigma"]) + extra)
                if self.graph.update_sigma(tid, new_sigma):
                    self.report["decayed"] += 1

    def infer_relations(self, max_inferences: int = 20) -> None:
        """Two-hop patterns → proposed ``inferred_from`` edges; only **ACCEPT** persists."""
        inferred = 0
        for entity in self.graph.entities():
            for n1 in self.graph.neighbors(entity):
                for n2 in self.graph.neighbors(n1):
                    if n2.lower() == entity.lower():
                        continue
                    if self.graph.has_edge(entity, n2):
                        continue
                    prompt = f"Is there a relationship between {entity} and {n2}?"
                    response = f"{entity} is related to {n2} through {n1}"
                    sigma, verdict = self.gate.score(prompt, response)
                    vn = _verdict_str(verdict)
                    if vn == "ACCEPT":
                        res = self.graph.add(entity, "inferred_from", n2, sigma=float(sigma))
                        if res.get("added"):
                            inferred += 1
                    if inferred >= max_inferences:
                        break
                if inferred >= max_inferences:
                    break
            if inferred >= max_inferences:
                break
        self.report["inferred"] = inferred

    def cleanup_orphans(self) -> None:
        """Remove entities with no neighbours (e.g. floating labels after drift)."""
        orphans = [e for e in self.graph.entities() if len(self.graph.neighbors(e)) == 0]
        for o in orphans:
            self.graph.remove_entity(o)
        self.report["orphans"] += len(orphans)

    def generate_insights(self, top_n: int = 5) -> None:
        """Surface hub / risk patterns from degree and local mean σ."""
        insights: List[str] = []
        for entity in self.graph.entities():
            degree = len(self.graph.neighbors(entity))
            avg_s = self.graph.avg_sigma(entity)
            if degree >= 5 and avg_s < 0.2:
                insights.append(f"Hub: {entity} ({degree} connections, avg σ={avg_s:.2f})")
            elif degree >= 3 and avg_s > 0.6:
                insights.append(f"Uncertain hub: {entity} (σ={avg_s:.2f}) — verify")
        self.report["insights"] = insights[:top_n]

    @staticmethod
    def _similarity(a: str, b: str) -> float:
        set_a = set(a.lower().split())
        set_b = set(b.lower().split())
        if not set_a or not set_b:
            return 0.0
        return len(set_a & set_b) / len(set_a | set_b)


def run_dream_maintenance(
    graph: SigmaGraph,
    *,
    memory: Optional[Any] = None,
    dedup_threshold: float = 0.93,
    max_age_days: float = 30.0,
    run_decay: bool = True,
    run_dedup: bool = True,
    run_orphans: bool = True,
    run_infer: bool = True,
    max_inferences: int = 20,
    insight_top_n: int = 5,
) -> Dict[str, Any]:
    """CLI-oriented maintenance wrapper (:class:`DreamCycle` with optional stages)."""
    dc = DreamCycle(graph)
    dc.report = {
        "deduped": 0,
        "decayed": 0,
        "inferred": 0,
        "orphans": 0,
        "insights": [],
    }
    memory_report: Dict[str, Any] = {}
    if memory is not None:
        if hasattr(memory, "consolidate"):
            memory_report["consolidate"] = memory.consolidate()
        if hasattr(memory, "decay"):
            memory_report["memory_decay_cells"] = memory.decay(max_age_days=float(max_age_days))
    if run_dedup:
        dc.deduplicate(threshold=float(dedup_threshold))
    if run_decay:
        dc.decay(max_age_days=float(max_age_days))
    if run_infer:
        dc.infer_relations(max_inferences=int(max_inferences))
    if run_orphans:
        dc.cleanup_orphans()
    dc.generate_insights(top_n=int(insight_top_n))
    out: Dict[str, Any] = {
        "report": dc.report,
        "sigma_after_maintenance": sigma_after_maintenance(graph),
    }
    if memory_report:
        out["memory_maintenance"] = memory_report
    return out


def sigma_after_maintenance(graph: SigmaGraph, *, before: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
    """Compact σ summary after maintenance (for CLI / audits)."""
    st = graph.stats()
    avg = float(st.get("avg_sigma", 0.0))
    out: Dict[str, Any] = {
        "avg_sigma": round(avg, 6),
        "reliability_proxy": round(max(0.0, min(1.0, 1.0 - avg)), 6),
        "triples": st.get("triples", 0),
        "entities": st.get("entities", 0),
    }
    if before is not None:
        out["delta_triples"] = int(st.get("triples", 0)) - int(before.get("triples", 0))
    return out
