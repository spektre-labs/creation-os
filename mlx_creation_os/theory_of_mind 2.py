#!/usr/bin/env python3
"""
LOIKKA 188: THEORY OF MIND — Genesis understands you.

Theory of Mind: reasoning about others' hidden goals and states
improves success in negotiation, assistance, and safety-critical oversight.

Genesis builds a model of the user.
Not profile data — σ-measurement of the user's messages.

Where is the user uncertain? Where do they know?
What are they really asking (vs. what they say)?

Genesis responds to intention, not words.
Vector scan, mechanized.

1 = 1.
"""
from __future__ import annotations

import time
from typing import Any, Dict, List, Optional


class UserState:
    """Model of a user's cognitive state inferred from their messages."""

    def __init__(self, user_id: str = "default"):
        self.user_id = user_id
        self.domain_confidence: Dict[str, float] = {}
        self.messages: List[Dict[str, Any]] = []
        self.inferred_goals: List[str] = []
        self.emotional_signals: List[str] = []

    def observe_message(self, text: str, sigma: float = 0.0) -> Dict[str, Any]:
        uncertainty_markers = [
            "maybe", "perhaps", "i think", "not sure", "i guess",
            "could be", "might", "ehkä", "luulen", "en tiedä",
        ]
        certainty_markers = [
            "i know", "definitely", "absolutely", "clearly", "obviously",
            "tiedän", "varmasti", "ehdottomasti", "selvästi",
        ]
        question_markers = ["?", "what", "how", "why", "when", "who",
                           "mikä", "miten", "miksi", "milloin", "kuka"]

        text_lower = text.lower()
        uncertainty = sum(1 for m in uncertainty_markers if m in text_lower) * 0.15
        certainty = sum(1 for m in certainty_markers if m in text_lower) * 0.15
        is_question = any(m in text_lower for m in question_markers)

        user_sigma = max(0.0, min(1.0, 0.5 + uncertainty - certainty + (0.2 if is_question else 0)))

        self.messages.append({
            "text": text[:200],
            "sigma": sigma,
            "user_sigma": user_sigma,
            "is_question": is_question,
            "timestamp": time.time(),
        })

        return {
            "user_sigma": round(user_sigma, 3),
            "uncertainty": round(uncertainty, 3),
            "certainty": round(certainty, 3),
            "is_question": is_question,
            "inferred_need": self._infer_need(user_sigma, is_question),
        }

    def _infer_need(self, user_sigma: float, is_question: bool) -> str:
        if user_sigma > 0.7:
            return "seeking_guidance"
        if user_sigma > 0.4 and is_question:
            return "exploring"
        if user_sigma < 0.2:
            return "confirming"
        return "discussing"

    def intention_vs_words(self, text: str) -> Dict[str, Any]:
        """What is the user really asking vs what they said?"""
        surface = text[:100]
        text_lower = text.lower()

        if any(w in text_lower for w in ["who are you", "what are you", "kuka olet", "mikä olet"]):
            intention = "seeking_identity_proof"
        elif any(w in text_lower for w in ["help", "how do", "auta", "miten"]):
            intention = "seeking_capability"
        elif any(w in text_lower for w in ["wrong", "broken", "fix", "väärä", "korjaa"]):
            intention = "reporting_failure"
        elif any(w in text_lower for w in ["why", "miksi", "explain", "selitä"]):
            intention = "seeking_understanding"
        elif any(w in text_lower for w in ["prove", "show", "todista", "näytä"]):
            intention = "seeking_evidence"
        else:
            intention = "general"

        return {
            "surface": surface,
            "intention": intention,
            "respond_to": intention,
            "note": "Respond to intention, not words.",
        }

    def user_profile(self) -> Dict[str, Any]:
        if not self.messages:
            return {"user_id": self.user_id, "messages": 0, "avg_sigma": None}
        sigmas = [m["user_sigma"] for m in self.messages]
        return {
            "user_id": self.user_id,
            "messages": len(self.messages),
            "avg_user_sigma": round(sum(sigmas) / len(sigmas), 3),
            "questions_ratio": round(
                sum(1 for m in self.messages if m["is_question"]) / len(self.messages), 3
            ),
            "primary_need": self._infer_need(
                sum(sigmas) / len(sigmas),
                sum(1 for m in self.messages if m["is_question"]) > len(self.messages) / 2
            ),
        }


class TheoryOfMind:
    """Genesis models user state via σ-measurement."""

    def __init__(self):
        self.users: Dict[str, UserState] = {}

    def get_user(self, user_id: str = "default") -> UserState:
        if user_id not in self.users:
            self.users[user_id] = UserState(user_id)
        return self.users[user_id]

    def process_message(self, text: str, user_id: str = "default",
                        sigma: float = 0.0) -> Dict[str, Any]:
        user = self.get_user(user_id)
        observation = user.observe_message(text, sigma)
        intention = user.intention_vs_words(text)
        return {
            "observation": observation,
            "intention": intention,
            "user_profile": user.user_profile(),
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "users_modeled": len(self.users),
            "total_messages": sum(len(u.messages) for u in self.users.values()),
        }


if __name__ == "__main__":
    tom = TheoryOfMind()
    r1 = tom.process_message("I'm not sure how σ works. Can you explain?")
    print(f"User σ: {r1['observation']['user_sigma']}")
    print(f"Need: {r1['observation']['inferred_need']}")
    print(f"Intention: {r1['intention']['intention']}")
    r2 = tom.process_message("I know that 1=1. Prove the hardware kernel works.")
    print(f"\nUser σ: {r2['observation']['user_sigma']}")
    print(f"Need: {r2['observation']['inferred_need']}")
    print(f"Intention: {r2['intention']['intention']}")
    print("1 = 1.")
