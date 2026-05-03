# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
# All rights reserved. See LICENSE for binding terms.
"""
v179 σ-conscious: **proxies** for integration / differentiation / self-model / time span — not consciousness.

**Does not claim phenomenal awareness.** σ measures coherence priors; proxies are audit cartoons.
See ``docs/CLAIM_DISCIPLINE.md`` — never merge lab proxy JSON with harness AUROC headlines.
"""
from __future__ import annotations

from typing import Any, Dict, List, Mapping, Sequence


class SigmaConsciousProxy:
    """Audit-only bundle; ``measure_consciousness_proxies`` is explicitly non-phenomenal."""

    def __init__(
        self,
        gate: Any,
        metacog: Any,
        drives: Any,
        memory: Any,
    ) -> None:
        self.gate = gate
        self.metacog = metacog
        self.drives = drives
        self.memory = memory

    def measure_consciousness_proxies(self) -> Dict[str, Any]:
        return {
            "disclaimer": "These are PROXIES — not consciousness, qualia, or subjective experience.",
            "integration": self.measure_integration(),
            "differentiation": self.measure_differentiation(),
            "self_model": self.measure_self_model(),
            "temporal_depth": self.measure_temporal_depth(),
            "metacognitive_accuracy": self.measure_metacognitive_accuracy(),
            "phi_proxy": self.estimate_phi_proxy(),
        }

    def measure_integration(self) -> Dict[str, Any]:
        gate_state: Mapping[str, Any] = {}
        if callable(getattr(self.gate, "get_state", None)):
            try:
                gate_state = self.gate.get_state()
            except Exception:
                gate_state = {}
        memory_state: List[Any] = []
        if callable(getattr(self.memory, "recall", None)):
            try:
                memory_state = list(self.memory.recall("", memory_type="semantic"))[:8]
            except Exception:
                memory_state = []
        drive_state = getattr(self.drives, "emotional_state", {}) or {}
        ema = float(gate_state.get("ema", 0.5)) if isinstance(gate_state, dict) else 0.5
        val = float(drive_state.get("valence", 0.0)) if isinstance(drive_state, dict) else 0.0
        correlation = abs(ema - val)
        mem_w = min(1.0, len(memory_state) / 8.0)
        return {
            "score": max(0.0, min(1.0, (1.0 - correlation) * 0.7 + mem_w * 0.3)),
            "description": "Toy coupling between gate EMA, affect log, and shallow memory hits (lab).",
        }

    def measure_differentiation(self) -> Dict[str, Any]:
        distinct_states = 3 + 5 + 3 + 3
        return {
            "score": min(1.0, distinct_states / 20.0),
            "distinct_states": distinct_states,
            "description": "Hand-counted coarse buckets (verdicts, cascade not traversed here).",
        }

    def measure_self_model(self) -> Dict[str, Any]:
        knowledge_inventory: Dict[str, Any] = {}
        if callable(getattr(self.metacog, "knowledge_inventory", None)):
            try:
                knowledge_inventory = self.metacog.knowledge_inventory()
            except Exception:
                knowledge_inventory = {}
        known_domains = [
            d
            for d, stats in knowledge_inventory.items()
            if isinstance(stats, dict) and float(stats.get("queries", 0)) > 5
        ]
        return {
            "score": min(1.0, len(known_domains) / 10.0),
            "known_domains": len(known_domains),
            "description": "Domains with enough logged metacog queries (lab threshold).",
        }

    def measure_temporal_depth(self) -> Dict[str, Any]:
        episodic_count = 0
        if hasattr(self.memory, "episodic"):
            episodic_count = len(getattr(self.memory, "episodic") or [])
        past_depth = min(1.0, episodic_count / 1000.0)
        future_depth = 0.25
        if callable(getattr(self.gate, "score", None)):
            try:
                s, _ = self.gate.score(
                    "planning horizon (lab)",
                    "How many meaningful lookahead steps before uncertainty dominates?",
                )
                future_depth = max(0.0, min(1.0, float(s)))
            except Exception:
                future_depth = 0.25
        return {
            "past_depth": past_depth,
            "future_depth": future_depth,
            "description": (
                "Past depth from episodic row count; future depth from σ on a fixed lookahead probe "
                "(not a trained world model)."
            ),
        }

    def measure_metacognitive_accuracy(self) -> Dict[str, Any]:
        return {
            "score": None,
            "source": "not_claimed_in_lab",
            "description": (
                "Calibration vs held-out errors requires a harness run; "
                "do not paste toy numbers here — see docs/CLAIM_DISCIPLINE.md."
            ),
        }

    def estimate_phi_proxy(self) -> Dict[str, Any]:
        integration = float(self.measure_integration()["score"])
        differentiation = float(self.measure_differentiation()["score"])
        phi = integration * differentiation
        return {
            "phi_proxy": phi,
            "note": "PRODUCT of lab proxy scores — not IIT Φ.",
            "interpretation": "high" if phi > 0.5 else "moderate" if phi > 0.2 else "low",
        }

    def perturbation_complexity(self, samples: Sequence[str]) -> Dict[str, Any]:
        """Lexical spread heuristic — not neural surprise."""
        import math
        from collections import Counter

        seq = [str(s) for s in samples if str(s).strip()]
        if not seq:
            return {"score": 0.0, "n": 0, "note": "empty input"}
        lengths = [len(s) for s in seq]
        mean_len = sum(lengths) / len(lengths)
        var = sum((L - mean_len) ** 2 for L in lengths) / len(lengths)
        toks = " ".join(seq).lower().split()
        counts = Counter(toks)
        if not toks:
            ent = 0.0
        else:
            ent = 0.0
            ln = len(toks)
            for c in counts.values():
                p = c / ln
                ent -= p * math.log2(p)
        score = max(0.0, min(1.0, math.tanh(var / 80.0) * 0.55 + (ent / 12.0) * 0.45))
        return {"score": float(score), "length_var": float(var), "token_entropy": float(ent), "n": len(seq)}

    def sigma_meta(self, narrative: str) -> Dict[str, Any]:
        """σ on a short self-report string — metacognitive stress read only."""
        s, v = self.gate.score("metacognitive_reflection", str(narrative)[:2000])
        return {"sigma": float(s), "verdict": str(v)}

    def phi_proxy(self) -> Dict[str, Any]:
        """Alias for :meth:`estimate_phi_proxy` (readable name for lab scripts)."""
        return self.estimate_phi_proxy()

    def self_reflect(self, question: str = "Who am I?") -> Dict[str, Any]:
        _ = question
        proxies = self.measure_consciousness_proxies()
        emotional = getattr(self.drives, "emotional_state", {})
        knowledge: Dict[str, Any] = {}
        if callable(getattr(self.metacog, "knowledge_inventory", None)):
            try:
                knowledge = self.metacog.knowledge_inventory()
            except Exception:
                knowledge = {}
        limitations: List[str] = []
        for d, s in knowledge.items():
            if isinstance(s, dict) and float(s.get("competence", 0.5)) < 0.3:
                limitations.append(str(d))
        return {
            "identity": "Creation OS σ-gated cognitive architecture (lab bundle)",
            "current_state": {
                "emotional": dict(emotional) if isinstance(emotional, dict) else emotional,
                "metacognitive": proxies["metacognitive_accuracy"],
                "phi_proxy": proxies["phi_proxy"],
            },
            "capabilities": list(knowledge.keys()),
            "limitations": limitations,
            "honest_assessment": (
                "I measure coherence proxies, not experience. "
                "1=1 is an invariant, not a feeling. "
                "No phenomenal consciousness is claimed."
            ),
        }


__all__ = ["SigmaConsciousProxy"]
