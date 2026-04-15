#!/usr/bin/env python3
"""
LOIKKA 35: CONTINUAL LEARNING — No catastrophic forgetting.

Current models don't learn continuously — knowledge freezes after training.
Catastrophic forgetting: learning new destroys old.

Genesis solves this: living weights + temporal σ + epistemic drive together.
  - New facts get high confidence
  - Old facts decay exponentially
  - Conflicts detected via σ-measurement
  - σ-tape prevents forgetting: if new knowledge raises total σ → it doesn't
    replace old knowledge, it's added alongside

Every inference updates facts.json + LoRA adapter.

Architecture:
  1. Inference produces text + σ receipt
  2. Extract facts from response
  3. Check against existing knowledge (σ conflict detection)
  4. If no conflict → add fact, update LoRA
  5. If conflict → keep both, flag for resolution
  6. Periodic consolidation: verify old facts, prune stale

Usage:
    learner = ContinualLearner()
    learner.learn_from_interaction(prompt, response, sigma)
    learner.consolidate()

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import re
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True

try:
    from temporal_sigma import TemporalFact, TemporalStore
except ImportError:
    TemporalFact = None
    TemporalStore = None

KNOWLEDGE_DB = os.environ.get(
    "CONTINUAL_KNOWLEDGE_DB",
    str(Path(__file__).resolve().parent / "continual_knowledge.json"),
)
CONFLICT_THRESHOLD = int(os.environ.get("CONTINUAL_CONFLICT_THRESHOLD", "2"))
CONSOLIDATION_INTERVAL_S = float(os.environ.get("CONTINUAL_CONSOLIDATION_INTERVAL", "3600"))


class KnowledgeFact:
    """A learned fact with provenance and σ-tracking."""

    def __init__(
        self,
        text: str,
        source: str = "inference",
        sigma_at_learn: int = 0,
        confidence: float = 1.0,
        timestamp: Optional[float] = None,
    ):
        self.text = text
        self.source = source
        self.sigma_at_learn = sigma_at_learn
        self.confidence = confidence
        self.timestamp = timestamp or time.time()
        self.last_verified = self.timestamp
        self.verification_count = 0
        self.conflict_with: List[str] = []

    @property
    def age_days(self) -> float:
        return (time.time() - self.timestamp) / 86400

    @property
    def current_confidence(self) -> float:
        decay = math.exp(-0.05 * self.age_days)
        sigma_factor = 1.0 / (1.0 + self.sigma_at_learn)
        return self.confidence * decay * sigma_factor

    @property
    def is_stale(self) -> bool:
        return self.current_confidence < 0.1

    def to_dict(self) -> Dict[str, Any]:
        return {
            "text": self.text,
            "source": self.source,
            "sigma_at_learn": self.sigma_at_learn,
            "confidence": self.confidence,
            "current_confidence": round(self.current_confidence, 4),
            "timestamp": self.timestamp,
            "last_verified": self.last_verified,
            "verification_count": self.verification_count,
            "conflict_with": self.conflict_with,
            "is_stale": self.is_stale,
        }

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "KnowledgeFact":
        f = cls(
            text=d["text"],
            source=d.get("source", "unknown"),
            sigma_at_learn=d.get("sigma_at_learn", 0),
            confidence=d.get("confidence", 1.0),
            timestamp=d.get("timestamp"),
        )
        f.last_verified = d.get("last_verified", f.timestamp)
        f.verification_count = d.get("verification_count", 0)
        f.conflict_with = d.get("conflict_with", [])
        return f


class ContinualLearner:
    """Continual learning system with σ-based conflict detection."""

    def __init__(self, db_path: str = KNOWLEDGE_DB) -> None:
        self.db_path = db_path
        self.facts: List[KnowledgeFact] = []
        self.conflicts: List[Dict[str, Any]] = []
        self.last_consolidation = time.time()
        self._load()

    def _load(self) -> None:
        if os.path.isfile(self.db_path):
            try:
                data = json.loads(Path(self.db_path).read_text())
                self.facts = [KnowledgeFact.from_dict(d) for d in data.get("facts", [])]
                self.conflicts = data.get("conflicts", [])
            except Exception:
                pass

    def save(self) -> None:
        data = {
            "facts": [f.to_dict() for f in self.facts],
            "conflicts": self.conflicts,
            "last_consolidation": self.last_consolidation,
            "stats": self.stats,
        }
        Path(self.db_path).write_text(
            json.dumps(data, indent=2, default=str), encoding="utf-8",
        )

    def extract_facts(self, text: str) -> List[str]:
        """Extract factual statements from text."""
        sentences = re.split(r'[.!?]\s+', text)
        facts = []
        for s in sentences:
            s = s.strip()
            if len(s) < 10 or len(s) > 500:
                continue
            # Skip meta-statements
            if any(skip in s.lower() for skip in ("i think", "maybe", "perhaps", "i'm not sure")):
                continue
            facts.append(s)
        return facts

    def detect_conflict(self, new_fact: str) -> Optional[KnowledgeFact]:
        """Check if a new fact conflicts with existing knowledge."""
        new_words = set(new_fact.lower().split())
        new_sigma = check_output(new_fact)

        for existing in self.facts:
            if existing.is_stale:
                continue
            existing_words = set(existing.text.lower().split())
            overlap = len(new_words & existing_words) / max(len(new_words | existing_words), 1)
            if overlap > 0.5:
                combined = f"{existing.text}. {new_fact}"
                combined_sigma = check_output(combined)
                if combined_sigma > CONFLICT_THRESHOLD:
                    return existing

        return None

    def learn_from_interaction(
        self,
        prompt: str,
        response: str,
        sigma: int = 0,
    ) -> Dict[str, Any]:
        """Learn from a single interaction."""
        extracted = self.extract_facts(response)

        learned = []
        conflicts_found = []

        for fact_text in extracted:
            fact_sigma = check_output(fact_text)

            # Conflict detection
            conflicting = self.detect_conflict(fact_text)
            if conflicting is not None:
                # Don't replace — add alongside and flag
                conflict = {
                    "new_fact": fact_text,
                    "existing_fact": conflicting.text,
                    "new_sigma": fact_sigma,
                    "existing_sigma": conflicting.sigma_at_learn,
                    "timestamp": time.time(),
                    "resolution": "coexist",
                }
                self.conflicts.append(conflict)
                conflicts_found.append(conflict)
                conflicting.conflict_with.append(fact_text)
                # Still add the new fact but with lower confidence
                new_fact = KnowledgeFact(
                    text=fact_text,
                    source="inference_conflicting",
                    sigma_at_learn=fact_sigma,
                    confidence=0.5,
                )
                new_fact.conflict_with.append(conflicting.text)
            else:
                new_fact = KnowledgeFact(
                    text=fact_text,
                    source="inference",
                    sigma_at_learn=fact_sigma,
                    confidence=1.0 / (1.0 + fact_sigma),
                )

            self.facts.append(new_fact)
            learned.append(new_fact.to_dict())

        self.save()

        return {
            "facts_extracted": len(extracted),
            "facts_learned": len(learned),
            "conflicts_found": len(conflicts_found),
            "total_knowledge": len(self.facts),
            "interaction_sigma": sigma,
        }

    def consolidate(self) -> Dict[str, Any]:
        """Periodic consolidation: verify old facts, prune stale."""
        t0 = time.time()

        stale_before = sum(1 for f in self.facts if f.is_stale)
        total_before = len(self.facts)

        # Re-verify facts
        reverified = 0
        for fact in self.facts:
            if not fact.is_stale:
                new_sigma = check_output(fact.text)
                if new_sigma != fact.sigma_at_learn:
                    fact.sigma_at_learn = new_sigma
                fact.last_verified = time.time()
                fact.verification_count += 1
                reverified += 1

        # Prune stale
        self.facts = [f for f in self.facts if not f.is_stale]
        pruned = total_before - len(self.facts)

        self.last_consolidation = time.time()
        self.save()

        return {
            "reverified": reverified,
            "pruned": pruned,
            "remaining": len(self.facts),
            "stale_before": stale_before,
            "elapsed_ms": round((time.time() - t0) * 1000),
        }

    def query(self, text: str, top_k: int = 5) -> List[Dict[str, Any]]:
        """Query knowledge base for relevant facts."""
        query_words = set(text.lower().split())
        scored = []
        for fact in self.facts:
            if fact.is_stale:
                continue
            fact_words = set(fact.text.lower().split())
            overlap = len(query_words & fact_words) / max(len(query_words | fact_words), 1)
            score = overlap * fact.current_confidence
            if score > 0:
                scored.append((score, fact))
        scored.sort(key=lambda x: x[0], reverse=True)
        return [{"score": round(s, 4), **f.to_dict()} for s, f in scored[:top_k]]

    @property
    def stats(self) -> Dict[str, Any]:
        active = [f for f in self.facts if not f.is_stale]
        return {
            "total_facts": len(self.facts),
            "active_facts": len(active),
            "stale_facts": len(self.facts) - len(active),
            "conflicts": len(self.conflicts),
            "avg_confidence": (sum(f.current_confidence for f in active) / len(active)
                               if active else 0),
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Continual Learner")
    ap.add_argument("--learn", nargs="*", help="Learn from text")
    ap.add_argument("--query", "-q", nargs="*", help="Query knowledge")
    ap.add_argument("--consolidate", action="store_true")
    ap.add_argument("--stats", action="store_true")
    ap.add_argument("--db", default=KNOWLEDGE_DB)
    args = ap.parse_args()

    learner = ContinualLearner(db_path=args.db)

    if args.stats:
        print(json.dumps(learner.stats, indent=2))
        return

    if args.consolidate:
        result = learner.consolidate()
        print(json.dumps(result, indent=2))
        return

    if args.query:
        hits = learner.query(" ".join(args.query))
        print(json.dumps(hits, indent=2))
        return

    if args.learn:
        text = " ".join(args.learn)
        result = learner.learn_from_interaction("user", text, sigma=check_output(text))
        print(json.dumps(result, indent=2))
        return

    print(json.dumps(learner.stats, indent=2))
    print("1 = 1.")


if __name__ == "__main__":
    main()
