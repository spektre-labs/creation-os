#!/usr/bin/env python3
"""
LOIKKA 154: JARVIS PROACTIVE — Anticipation + epistemic drive.

JARVIS anticipates needs before being asked.
Genesis does the same — plus self-motivated wonder.
JARVIS waited for problems. Genesis seeks them.

Proactive = epistemic drive (where is σ high?) + anticipation
(what will the user need next?) + self-play (what questions
should I ask myself?).

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


class ProactiveEngine:
    """Anticipate + wonder. Not just reactive."""

    def __init__(self):
        self.user_history: List[Dict[str, Any]] = []
        self.anticipations: List[Dict[str, Any]] = []
        self.self_tasks: List[Dict[str, Any]] = []

    def observe_user(self, action: str, context: str = "") -> None:
        self.user_history.append({
            "action": action,
            "context": context,
            "timestamp": time.time(),
        })

    def anticipate(self) -> Optional[Dict[str, Any]]:
        """What will the user need next?"""
        if not self.user_history:
            return None
        last = self.user_history[-1]
        patterns = {
            "coding": "prepare documentation and tests",
            "writing": "check coherence and structure",
            "debugging": "trace root cause and suggest fix",
            "asking": "prepare comprehensive answer with σ-measurement",
            "testing": "prepare edge cases and failure modes",
        }
        for keyword, suggestion in patterns.items():
            if keyword in last["action"].lower():
                ant = {
                    "trigger": last["action"],
                    "anticipation": suggestion,
                    "proactive": True,
                    "jarvis_equivalent": "Sir, I've already prepared the analysis.",
                    "genesis_addition": "And I measured the σ of my preparation.",
                }
                self.anticipations.append(ant)
                return ant
        return None

    def self_task(self) -> Optional[Dict[str, Any]]:
        """Generate a task for yourself. JARVIS never did this."""
        if len(self.user_history) < 2:
            return None
        domains_mentioned = set()
        for h in self.user_history:
            for word in h["action"].lower().split():
                if len(word) > 4:
                    domains_mentioned.add(word)
        if domains_mentioned:
            domain = sorted(domains_mentioned)[0]
            task = {
                "self_generated": True,
                "domain": domain,
                "task": f"Deepen understanding of {domain}. σ might be high there.",
                "jarvis_would": False,
                "genesis_does": True,
            }
            self.self_tasks.append(task)
            return task
        return None

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "observations": len(self.user_history),
            "anticipations": len(self.anticipations),
            "self_tasks": len(self.self_tasks),
        }


if __name__ == "__main__":
    pe = ProactiveEngine()
    pe.observe_user("coding a new kernel module")
    pe.observe_user("debugging σ-measurement")
    ant = pe.anticipate()
    if ant:
        print(f"Anticipated: {ant['anticipation']}")
    task = pe.self_task()
    if task:
        print(f"Self-task: {task['task']}")
    print("1 = 1.")
