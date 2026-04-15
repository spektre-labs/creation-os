#!/usr/bin/env python3
"""
LOIKKA 184: CHILD-LIKE LEARNING — Grow like a child.

A mind that learns, reasons, adapts, and grows incrementally — like a child.

Genesis is not trained once and frozen. It grows.
Starts with minimal knowledge (Codex + kernel).
Each interaction teaches. Epistemic drive guides curiosity:
what Genesis doesn't know → it asks → it learns → σ drops on that domain.
Living weights record the learning. Temporal σ tracks development.

Like a child: doesn't know everything — but learns fast and remembers.

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


class KnowledgeState:
    """What the child-mind knows at any point in time."""

    def __init__(self, name: str, sigma: float = 1.0):
        self.name = name
        self.sigma = sigma
        self.learned_at: Optional[float] = None
        self.reinforced = 0
        self.last_accessed: Optional[float] = None

    def learn(self, new_sigma: float) -> float:
        improvement = self.sigma - new_sigma
        self.sigma = min(self.sigma, new_sigma)
        self.learned_at = time.time()
        self.reinforced += 1
        self.last_accessed = time.time()
        return improvement

    def access(self) -> float:
        self.last_accessed = time.time()
        self.reinforced += 1
        return self.sigma

    @property
    def maturity(self) -> str:
        if self.sigma > 0.8:
            return "unknown"
        if self.sigma > 0.5:
            return "aware"
        if self.sigma > 0.1:
            return "learning"
        if self.sigma > 0.01:
            return "competent"
        return "mastered"


class ChildLearner:
    """Genesis grows incrementally, like a child."""

    def __init__(self):
        self.knowledge: Dict[str, KnowledgeState] = {}
        self.interactions = 0
        self.questions_asked: List[str] = []
        self.sigma_timeline: List[Dict[str, Any]] = []

    def _ensure_domain(self, domain: str) -> KnowledgeState:
        if domain not in self.knowledge:
            self.knowledge[domain] = KnowledgeState(domain)
        return self.knowledge[domain]

    def encounter(self, domain: str, sigma: float) -> Dict[str, Any]:
        """Learn from an interaction in a domain."""
        self.interactions += 1
        ks = self._ensure_domain(domain)
        old_sigma = ks.sigma
        improvement = ks.learn(sigma)
        self.sigma_timeline.append({
            "interaction": self.interactions,
            "domain": domain,
            "sigma_before": old_sigma,
            "sigma_after": ks.sigma,
            "improvement": improvement,
            "timestamp": time.time(),
        })
        return {
            "domain": domain,
            "sigma_before": old_sigma,
            "sigma_after": ks.sigma,
            "improvement": round(improvement, 4),
            "maturity": ks.maturity,
            "interactions": self.interactions,
        }

    def curiosity_drive(self) -> Dict[str, Any]:
        """What should Genesis explore next? Epistemic drive."""
        if not self.knowledge:
            return {"next": "everything", "sigma": 1.0, "reason": "tabula rasa"}
        unknowns = [(k, v) for k, v in self.knowledge.items() if v.sigma > 0.5]
        if unknowns:
            worst = max(unknowns, key=lambda x: x[1].sigma)
            question = f"I don't know enough about {worst[0]}. σ={worst[1].sigma:.2f}"
            self.questions_asked.append(question)
            return {
                "next": worst[0],
                "sigma": worst[1].sigma,
                "reason": question,
                "n_unknowns": len(unknowns),
            }
        learning = [(k, v) for k, v in self.knowledge.items() if v.sigma > 0.01]
        if learning:
            target = max(learning, key=lambda x: x[1].sigma)
            return {
                "next": target[0],
                "sigma": target[1].sigma,
                "reason": f"Deepening understanding of {target[0]}",
                "n_learning": len(learning),
            }
        return {"next": None, "sigma": 0.0, "reason": "All domains mastered. Rest."}

    def development_report(self) -> Dict[str, Any]:
        maturity_counts = {"unknown": 0, "aware": 0, "learning": 0, "competent": 0, "mastered": 0}
        for ks in self.knowledge.values():
            maturity_counts[ks.maturity] += 1
        avg_sigma = sum(v.sigma for v in self.knowledge.values()) / max(len(self.knowledge), 1)
        return {
            "domains": len(self.knowledge),
            "interactions": self.interactions,
            "maturity": maturity_counts,
            "avg_sigma": round(avg_sigma, 4),
            "questions_asked": len(self.questions_asked),
            "stage": self._development_stage(avg_sigma),
        }

    def _development_stage(self, avg_sigma: float) -> str:
        if avg_sigma > 0.8:
            return "infant"
        if avg_sigma > 0.5:
            return "toddler"
        if avg_sigma > 0.2:
            return "child"
        if avg_sigma > 0.05:
            return "adolescent"
        return "adult"

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "domains": len(self.knowledge),
            "interactions": self.interactions,
            "questions": len(self.questions_asked),
        }


if __name__ == "__main__":
    cl = ChildLearner()
    for domain, sigma in [("language", 0.3), ("math", 0.7), ("identity", 0.01), ("physics", 0.9)]:
        r = cl.encounter(domain, sigma)
        print(f"  {domain:12s} σ={r['sigma_after']:.2f} [{r['maturity']}]")
    curiosity = cl.curiosity_drive()
    print(f"\nCuriosity: {curiosity['reason']}")
    report = cl.development_report()
    print(f"Stage: {report['stage']} (avg σ={report['avg_sigma']:.3f})")
    print("1 = 1.")
