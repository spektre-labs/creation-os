#!/usr/bin/env python3
"""
TEMPORAL σ — Memory across time. Facts age and decay.

σ is not static. The same answer may be coherent today and incoherent
tomorrow because facts change. Temporal σ: every fact gets a timestamp.
Epistemic drive periodically checks: are old facts still coherent?
If not → σ rises → fact marked as stale.

Genesis knows what it no longer knows.

Architecture:
  1. TemporalFact: {text, timestamp, σ_at_creation, last_verified, ttl, confidence}
  2. TemporalStore: facts.json with time-aware σ
  3. Decay function: confidence = exp(-λ * age_days) where λ = σ-dependent
  4. Periodic sweep: re-verify old facts, update σ, prune stale
  5. Query-time: temporal_sigma(fact) = base_sigma + age_penalty

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

try:
    from creation_os_core import check_output, is_coherent
except ImportError:
    def check_output(t: str) -> int: return 0
    def is_coherent(t: str) -> bool: return True

TEMPORAL_DB = os.environ.get(
    "TEMPORAL_SIGMA_DB",
    str(Path(__file__).resolve().parent / "temporal_facts.json"),
)
DEFAULT_TTL_DAYS = 30.0
DECAY_LAMBDA = 0.05  # Confidence half-life ~14 days


class TemporalFact:
    """A fact with temporal σ-awareness."""

    def __init__(
        self,
        text: str,
        sigma: int = 0,
        timestamp: Optional[float] = None,
        ttl_days: float = DEFAULT_TTL_DAYS,
        source: str = "",
        category: str = "general",
    ):
        self.text = text
        self.sigma_at_creation = sigma
        self.timestamp = timestamp or time.time()
        self.last_verified = self.timestamp
        self.ttl_days = ttl_days
        self.source = source
        self.category = category
        self.verification_count = 0
        self.stale = False

    @property
    def age_days(self) -> float:
        return (time.time() - self.timestamp) / 86400.0

    @property
    def since_verified_days(self) -> float:
        return (time.time() - self.last_verified) / 86400.0

    @property
    def confidence(self) -> float:
        """Temporal confidence: decays exponentially with age."""
        age = self.since_verified_days
        base = math.exp(-DECAY_LAMBDA * age)
        sigma_penalty = self.sigma_at_creation * 0.1
        return max(0.0, min(1.0, base - sigma_penalty))

    @property
    def temporal_sigma(self) -> float:
        """σ adjusted for time: base σ + age penalty."""
        age_penalty = max(0, self.age_days - self.ttl_days) * 0.1
        staleness_penalty = 2 if self.stale else 0
        return self.sigma_at_creation + age_penalty + staleness_penalty

    @property
    def is_expired(self) -> bool:
        return self.age_days > self.ttl_days * 2

    def verify(self) -> int:
        """Re-verify this fact. Returns current σ."""
        sigma = check_output(self.text)
        coherent = is_coherent(self.text)
        self.sigma_at_creation = sigma
        self.last_verified = time.time()
        self.verification_count += 1
        self.stale = not coherent or sigma > 2
        return sigma

    def to_dict(self) -> Dict[str, Any]:
        return {
            "text": self.text,
            "sigma_at_creation": self.sigma_at_creation,
            "timestamp": self.timestamp,
            "last_verified": self.last_verified,
            "ttl_days": self.ttl_days,
            "source": self.source,
            "category": self.category,
            "verification_count": self.verification_count,
            "stale": self.stale,
            "confidence": round(self.confidence, 4),
            "temporal_sigma": round(self.temporal_sigma, 3),
            "age_days": round(self.age_days, 2),
        }

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "TemporalFact":
        f = cls(
            text=d["text"],
            sigma=d.get("sigma_at_creation", 0),
            timestamp=d.get("timestamp", time.time()),
            ttl_days=d.get("ttl_days", DEFAULT_TTL_DAYS),
            source=d.get("source", ""),
            category=d.get("category", "general"),
        )
        f.last_verified = d.get("last_verified", f.timestamp)
        f.verification_count = d.get("verification_count", 0)
        f.stale = d.get("stale", False)
        return f


class TemporalStore:
    """Persistent store for time-aware facts."""

    def __init__(self, db_path: str = TEMPORAL_DB):
        self.db_path = db_path
        self.facts: List[TemporalFact] = []
        self._load()

    def _load(self) -> None:
        if os.path.isfile(self.db_path):
            with open(self.db_path) as f:
                data = json.load(f)
            self.facts = [TemporalFact.from_dict(d) for d in data]

    def save(self) -> None:
        with open(self.db_path, "w") as f:
            json.dump([fact.to_dict() for fact in self.facts], f, indent=2)

    def add(self, text: str, source: str = "", category: str = "general",
            ttl_days: float = DEFAULT_TTL_DAYS) -> TemporalFact:
        sigma = check_output(text)
        fact = TemporalFact(text, sigma=sigma, source=source,
                            category=category, ttl_days=ttl_days)
        self.facts.append(fact)
        self.save()
        return fact

    def query(self, text: str, top_k: int = 5) -> List[TemporalFact]:
        """Query facts, sorted by relevance * confidence."""
        query_words = set(text.lower().split())
        scored = []
        for fact in self.facts:
            if fact.is_expired:
                continue
            fact_words = set(fact.text.lower().split())
            overlap = len(query_words & fact_words) / max(len(query_words | fact_words), 1)
            score = overlap * fact.confidence
            scored.append((score, fact))
        scored.sort(key=lambda x: -x[0])
        return [f for _, f in scored[:top_k]]

    def sweep(self) -> Dict[str, Any]:
        """Re-verify all facts. Mark stale ones. Prune expired."""
        verified = 0
        stale_count = 0
        pruned = 0
        remaining = []

        for fact in self.facts:
            if fact.is_expired:
                pruned += 1
                continue
            if fact.since_verified_days > 1.0:  # Re-verify if > 1 day old
                fact.verify()
                verified += 1
            if fact.stale:
                stale_count += 1
            remaining.append(fact)

        self.facts = remaining
        self.save()

        return {
            "total": len(remaining),
            "verified": verified,
            "stale": stale_count,
            "pruned": pruned,
        }

    @property
    def stats(self) -> Dict[str, Any]:
        if not self.facts:
            return {"total": 0}
        confidences = [f.confidence for f in self.facts]
        sigmas = [f.temporal_sigma for f in self.facts]
        return {
            "total": len(self.facts),
            "stale": sum(1 for f in self.facts if f.stale),
            "expired": sum(1 for f in self.facts if f.is_expired),
            "avg_confidence": round(sum(confidences) / len(confidences), 4),
            "avg_temporal_sigma": round(sum(sigmas) / len(sigmas), 3),
            "categories": list(set(f.category for f in self.facts)),
        }


def temporal_sigma_for_text(text: str, store: Optional[TemporalStore] = None) -> float:
    """Get temporal σ for a text, considering known facts."""
    if store is None:
        store = TemporalStore()

    base_sigma = check_output(text)
    relevant = store.query(text, top_k=3)
    if not relevant:
        return float(base_sigma)

    # Adjust σ based on supporting/contradicting facts
    max_confidence = max(f.confidence for f in relevant)
    if max_confidence > 0.8:
        return float(base_sigma)  # Well-supported
    elif max_confidence < 0.3:
        return float(base_sigma) + 1.0  # Weak support → higher σ

    return float(base_sigma) + (1.0 - max_confidence) * 0.5


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="Temporal σ — facts that age")
    ap.add_argument("--add", nargs="*", help="Add a fact")
    ap.add_argument("--query", "-q", nargs="*", help="Query facts")
    ap.add_argument("--sweep", action="store_true", help="Re-verify all facts")
    ap.add_argument("--stats", action="store_true")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--db", default=TEMPORAL_DB)
    args = ap.parse_args()

    store = TemporalStore(args.db)

    if args.add:
        text = " ".join(args.add)
        fact = store.add(text)
        print(json.dumps(fact.to_dict(), indent=2))

    if args.query:
        text = " ".join(args.query)
        results = store.query(text)
        for f in results:
            print(f"  [{f.confidence:.2f}] σ_t={f.temporal_sigma:.1f} | {f.text[:80]}")

    if args.sweep:
        result = store.sweep()
        print(json.dumps(result, indent=2))

    if args.stats:
        print(json.dumps(store.stats, indent=2))

    if args.list:
        for f in store.facts:
            age = f"({f.age_days:.0f}d)" if f.age_days > 1 else "(new)"
            stale = " [STALE]" if f.stale else ""
            print(f"  [{f.confidence:.2f}] {age}{stale} {f.text[:80]}")
