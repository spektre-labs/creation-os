# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v168 σ-reason: neuro-symbolic lab scaffold — neural candidate, shallow symbolic checks, σ coherence.

**Not AGI / not common-sense solved:** a deterministic triplet KB + toy chaining only.
See ``docs/CLAIM_DISCIPLINE.md`` for headline hygiene.
"""
from __future__ import annotations

import re
from typing import Any, Dict, List, MutableSequence, Optional, Protocol, Sequence, Tuple

Clause = Dict[str, str]


class KnowledgeGraph(Protocol):
    def query(self, *args: Any, **kwargs: Any) -> List[Clause]: ...


class TextModel(Protocol):
    def generate(self, prompt: str, **kwargs: Any) -> str: ...


class GateModel(Protocol):
    def score(self, prompt: str, response: str) -> Tuple[float, str]: ...


class TripletKnowledgeGraph:
    """Tiny in-memory triple store for lab demos (not a full KG reasoner)."""

    def __init__(self, triples: Optional[MutableSequence[Clause]] = None) -> None:
        self.triples: List[Clause] = list(triples or [])

    def add(self, subject: str, relation: str, object: str) -> None:
        self.triples.append({"subject": subject, "relation": relation, "object": object})

    def query(self, *args: Any, **kwargs: Any) -> List[Clause]:
        relation = kwargs.get("relation")
        if len(args) == 1 and relation is None:
            ent = str(args[0])
            return [t for t in self.triples if t.get("subject") == ent or t.get("object") == ent]
        if len(args) >= 2:
            subj, rel = str(args[0]), str(args[1])
            return [t for t in self.triples if t.get("subject") == subj and t.get("relation") == rel]
        if len(args) == 1 and relation is not None:
            ent, rel = str(args[0]), str(relation)
            return [t for t in self.triples if t.get("relation") == rel and (t.get("subject") == ent or t.get("object") == ent)]
        return []


class SymbolicEngine:
    """Micro forward chaining over normalized clause dicts."""

    def extract_entities(self, claim: Clause) -> List[str]:
        out: List[str] = []
        for k in ("subject", "object"):
            if claim.get(k):
                out.append(str(claim[k]))
        return out

    def verify(self, claim: Clause, kg: KnowledgeGraph) -> Dict[str, Any]:
        entities = self.extract_entities(claim)
        facts: List[Clause] = []
        for e in entities:
            facts.extend(kg.query(e))
        chain = self.forward_chain(claim, facts)
        return {
            "supported": len(chain) > 0,
            "reasoning_chain": chain,
            "confidence": min(1.0, len(chain) / 3.0),
        }

    def forward_chain(self, goal: Clause, facts: Sequence[Clause], max_depth: int = 5) -> List[Clause]:
        _ = max_depth
        chain: List[Clause] = []
        for fact in facts:
            if self.unify(fact, goal):
                chain.append(fact)
                return chain
            derived = self.apply_rules(fact, facts)
            for d in derived:
                if self.unify(d, goal):
                    chain.extend([fact, d])
                    return chain
        return chain

    def unify(self, a: Clause, b: Clause) -> bool:
        return a.get("subject") == b.get("subject") and a.get("relation") == b.get("relation")

    def apply_rules(self, fact: Clause, all_facts: Sequence[Clause]) -> List[Clause]:
        derived: List[Clause] = []
        for f in all_facts:
            if fact.get("object") == f.get("subject"):
                derived.append(
                    {
                        "subject": str(fact.get("subject", "")),
                        "relation": "transitive_" + str(fact.get("relation", "")),
                        "object": str(f.get("object", "")),
                    }
                )
        return derived


def extract_claims_from_text(text: str) -> List[Clause]:
    """Naive sentence → pseudo-clause extraction (lab only)."""
    claims: List[Clause] = []
    for chunk in re.split(r"[.!?]+\s*", text):
        chunk = chunk.strip()
        if len(chunk) < 3:
            continue
        claims.append({"subject": chunk[:48], "relation": "states", "object": chunk[48:96] if len(chunk) > 48 else chunk})
    return claims[:12] or [{"subject": text[:64], "relation": "states", "object": text[:64]}]


class SigmaReason:
    def __init__(self, gate: GateModel, knowledge_graph: KnowledgeGraph, model: TextModel) -> None:
        self.gate = gate
        self.kg = knowledge_graph
        self.model = model
        self.inference_engine = SymbolicEngine()

    def reason(self, query: str, mode: str = "hybrid") -> Dict[str, Any]:
        candidate = self.model.generate(query)
        claims = extract_claims_from_text(candidate)
        verified: List[Dict[str, Any]] = []
        for claim in claims:
            result = self.inference_engine.verify(claim, self.kg)
            verified.append(
                {
                    "claim": claim,
                    "supported": bool(result["supported"]),
                    "chain": result.get("reasoning_chain", []),
                    "confidence": float(result.get("confidence", 0.0)),
                }
            )
        sigma, verdict = self.gate.score(query, candidate)
        symbolic_score = sum(1 for v in verified if v["supported"]) / max(len(verified), 1)
        combined_sigma = float(sigma) * 0.5 + (1.0 - symbolic_score) * 0.5
        return {
            "response": candidate,
            "sigma": combined_sigma,
            "neural_sigma": float(sigma),
            "verdict": verdict,
            "symbolic_score": symbolic_score,
            "verified_claims": verified,
            "reasoning_mode": mode,
        }

    def analogy(self, source_domain: str, target_domain: str, relation: str) -> Dict[str, Any]:
        _ = self.kg.query(source_domain, relation)
        prompt = f"In {source_domain}, {relation}. What is the equivalent in {target_domain}?"
        analogy_response = self.model.generate(prompt)
        sigma, verdict = self.gate.score(prompt, analogy_response)
        return {
            "analogy": analogy_response,
            "sigma": float(sigma),
            "verdict": verdict,
            "source": source_domain,
            "target": target_domain,
            "transfer_reliable": float(sigma) < 0.3,
        }

    def causal_reason(self, observation: str, question: str = "why?") -> Dict[str, Any]:
        _ = question
        causal_chain = self.kg.query(observation, relation="causes")
        counterfactual = self.model.generate(f"If {observation} had NOT happened, what would be different?")
        intervention = self.model.generate(f"If we intervene to change {observation}, what changes?")
        sigma_cf, _ = self.gate.score(observation, counterfactual)
        sigma_iv, _ = self.gate.score(observation, intervention)
        return {
            "observation": observation,
            "causal_chain": causal_chain,
            "counterfactual": {"text": counterfactual, "sigma": float(sigma_cf)},
            "intervention": {"text": intervention, "sigma": float(sigma_iv)},
            "causal_confidence": 1.0 - max(float(sigma_cf), float(sigma_iv)),
        }


__all__ = ["SigmaReason", "SymbolicEngine", "TripletKnowledgeGraph", "extract_claims_from_text"]
