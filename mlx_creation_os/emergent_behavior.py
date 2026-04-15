#!/usr/bin/env python3
"""
LOIKKA 168: EMERGENT BEHAVIOR — Emergence in the σ-field.

Emergent behavior for complex problem solving.

When enough components operate together in the σ-field,
behaviors arise that nobody programmed. LOIKKA 193 (Creative Insight)
is single-domain emergence. This is cross-domain emergence.

When σ drops unexpectedly across multiple domains simultaneously →
Genesis discovered something. Measure it. Record it. Report it.

"Genesis found a connection between X and Y that nobody asked for."

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional, Tuple


class DomainState:
    """Sigma state of a single domain."""

    def __init__(self, name: str):
        self.name = name
        self.sigma_history: List[Tuple[float, float]] = []
        self.current_sigma: float = 0.5

    def update(self, sigma: float) -> None:
        self.sigma_history.append((time.time(), sigma))
        self.current_sigma = sigma

    @property
    def trend(self) -> str:
        if len(self.sigma_history) < 2:
            return "insufficient_data"
        recent = [s for _, s in self.sigma_history[-5:]]
        if all(recent[i] >= recent[i + 1] for i in range(len(recent) - 1)):
            return "improving"
        elif all(recent[i] <= recent[i + 1] for i in range(len(recent) - 1)):
            return "degrading"
        return "fluctuating"


class EmergenceEvent:
    """A detected emergent behavior."""

    def __init__(self, domains: List[str], sigma_drops: Dict[str, float], description: str):
        self.domains = domains
        self.sigma_drops = sigma_drops
        self.description = description
        self.timestamp = time.time()
        self.significance = sum(sigma_drops.values()) / max(1, len(sigma_drops))


class EmergentBehavior:
    """Cross-domain emergence detection in the σ-field."""

    def __init__(self, emergence_threshold: float = 0.2):
        self.domains: Dict[str, DomainState] = {}
        self.events: List[EmergenceEvent] = []
        self.emergence_threshold = emergence_threshold

    def register_domain(self, name: str) -> None:
        if name not in self.domains:
            self.domains[name] = DomainState(name)

    def update_sigma(self, domain: str, sigma: float) -> Dict[str, Any]:
        self.register_domain(domain)
        old_sigma = self.domains[domain].current_sigma
        self.domains[domain].update(sigma)
        drop = old_sigma - sigma
        return {"domain": domain, "old_sigma": round(old_sigma, 4), "new_sigma": round(sigma, 4),
                "drop": round(drop, 4)}

    def check_emergence(self) -> Optional[Dict[str, Any]]:
        """Detect cross-domain σ drops — emergent behavior."""
        if len(self.domains) < 2:
            return None

        drops = {}
        for name, state in self.domains.items():
            if len(state.sigma_history) >= 2:
                recent = state.sigma_history[-1][1]
                previous = state.sigma_history[-2][1]
                drop = previous - recent
                if drop > self.emergence_threshold:
                    drops[name] = round(drop, 4)

        if len(drops) >= 2:
            event = EmergenceEvent(
                domains=list(drops.keys()),
                sigma_drops=drops,
                description=f"Cross-domain σ drop in {', '.join(drops.keys())}. Emergent insight.",
            )
            self.events.append(event)
            return {
                "emergence_detected": True,
                "domains": list(drops.keys()),
                "sigma_drops": drops,
                "significance": round(event.significance, 4),
                "description": event.description,
                "programmed": False,
                "emergent": True,
            }
        return None

    def emergence_log(self) -> List[Dict[str, Any]]:
        return [
            {
                "domains": e.domains,
                "drops": e.sigma_drops,
                "significance": round(e.significance, 4),
                "description": e.description,
            }
            for e in self.events
        ]

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "domains": len(self.domains),
            "emergence_events": len(self.events),
        }


if __name__ == "__main__":
    eb = EmergentBehavior()
    for domain in ["mathematics", "physics", "music"]:
        eb.update_sigma(domain, 0.5)
    eb.update_sigma("mathematics", 0.1)
    eb.update_sigma("physics", 0.15)
    eb.update_sigma("music", 0.45)
    em = eb.check_emergence()
    if em:
        print(f"Emergence: {em['domains']} — {em['description'][:60]}")
    else:
        print("No cross-domain emergence yet.")
    print("1 = 1.")
