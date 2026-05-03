# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v169 σ-metacog: introspection + coarse epistemic vs aleatoric lab tagging (σ-gated).

**Not calibrated uncertainty decomposition:** variance over repeated samples is a toy prior.
"""
from __future__ import annotations

import re
from typing import Any, Callable, Dict, List


class SigmaMetaCognition:
    def __init__(self, gate: Any, model: Any) -> None:
        self.gate = gate
        self.model = model
        self.knowledge_map: Dict[str, Dict[str, float]] = {}

    def introspect(self, query: str) -> Dict[str, Any]:
        response = self.model.generate(query)
        sigma, verdict = self.gate.score(query, response)
        uncertainty_type = self.classify_uncertainty(query, response, float(sigma))
        strategy = self.suggest_strategy(uncertainty_type, float(sigma))
        domain = self.detect_domain(query)
        self.update_knowledge_map(domain, float(sigma))
        inv = self.knowledge_map.get(domain, {})
        return {
            "sigma": float(sigma),
            "verdict": verdict,
            "uncertainty_type": uncertainty_type,
            "strategy": strategy,
            "domain": domain,
            "domain_competence": 1.0 - float(inv.get("avg_sigma", 0.5)),
            "i_know_that_i_dont_know": float(sigma) > 0.5,
        }

    def classify_uncertainty(self, query: str, response: str, sigma: float) -> str:
        samples: List[float] = []
        gen: Callable[..., str] = self.model.generate
        for i in range(5):
            try:
                r = gen(query, temperature=0.7 + i * 0.02)
            except TypeError:
                r = gen(f"{query} [sample {i}]")
            s, _ = self.gate.score(query, r)
            samples.append(float(s))
        mean = sum(samples) / max(len(samples), 1)
        var = sum((s - mean) ** 2 for s in samples) / max(len(samples), 1)
        if var > 0.05:
            return "epistemic"
        if sigma > 0.3:
            return "aleatoric"
        return "confident"

    def suggest_strategy(self, uncertainty_type: str, sigma: float) -> Dict[str, Any]:
        _ = sigma
        strategies: Dict[str, Dict[str, Any]] = {
            "epistemic": {
                "action": "seek_more_information",
                "methods": ["search", "ask_expert", "retrieve_from_engram", "TTT_adapt"],
                "rationale": "Uncertainty from missing knowledge — more data may help (lab tag).",
            },
            "aleatoric": {
                "action": "acknowledge_uncertainty",
                "methods": ["report_range", "probabilistic_answer", "ABSTAIN_if_critical"],
                "rationale": "Low cross-sample disagreement but high σ — treat as irreducible noise prior.",
            },
            "confident": {
                "action": "proceed",
                "methods": ["ACCEPT"],
                "rationale": "σ low and samples stable in this toy test.",
            },
        }
        return strategies.get(uncertainty_type, strategies["epistemic"])

    def knowledge_inventory(self) -> Dict[str, Dict[str, Any]]:
        return {
            domain: {
                "competence": 1.0 - float(stats.get("avg_sigma", 0.5)),
                "queries": int(stats.get("count", 0)),
                "trend": str(stats.get("trend", "stable")),
            }
            for domain, stats in self.knowledge_map.items()
        }

    def update_knowledge_map(self, domain: str, sigma: float) -> None:
        if domain not in self.knowledge_map:
            self.knowledge_map[domain] = {"avg_sigma": sigma, "count": 1.0, "trend": "stable"}
        else:
            m = self.knowledge_map[domain]
            m["avg_sigma"] = 0.05 * sigma + 0.95 * float(m["avg_sigma"])
            m["count"] = float(m["count"]) + 1.0

    def detect_domain(self, query: str) -> str:
        q = query.lower()
        if any(x in q for x in ("physics", "force", "energy")):
            return "physics"
        if any(x in q for x in ("bio", "cell", "dna")):
            return "biology"
        if any(x in q for x in ("code", "python", "algorithm")):
            return "cs"
        stub = re.sub(r"[^a-z]+", "", q)[:12]
        return stub or "general"


__all__ = ["SigmaMetaCognition"]
