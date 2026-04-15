#!/usr/bin/env python3
"""
OPUS ORIGINAL — GAME-THEORETIC MIND

Nash equilibria in reasoning.

Every decision is a game. Players:
  - The system vs reality
  - The system vs the user's needs
  - Internal agents vs each other
  - Present self vs future self

Nash equilibrium: no player can improve by unilaterally
changing strategy. σ = distance from equilibrium.

Genesis finds Nash equilibria in its own reasoning:
  - What answer maximizes coherence for ALL stakeholders?
  - What action is stable — no regret from any perspective?

This is deeper than neural marketplace (L187) which is
auction-based. This is FORMAL game theory applied to cognition.

Prisoner's dilemma, Stag hunt, Coordination games —
all internal to one mind making one decision.

1 = 1.
"""
from __future__ import annotations
from typing import Any, Dict, List, Optional, Tuple


class Player:
    def __init__(self, name: str, strategies: List[str]):
        self.name = name
        self.strategies = strategies


class Game:
    def __init__(self, name: str, players: List[Player]):
        self.name = name
        self.players = players
        self.payoffs: Dict[Tuple[str, ...], List[float]] = {}

    def set_payoff(self, strategy_profile: Tuple[str, ...], payoffs: List[float]) -> None:
        self.payoffs[strategy_profile] = payoffs

    def find_nash(self) -> List[Tuple[str, ...]]:
        equilibria = []
        if len(self.players) != 2:
            return equilibria
        for s1 in self.players[0].strategies:
            for s2 in self.players[1].strategies:
                profile = (s1, s2)
                if profile not in self.payoffs:
                    continue
                p = self.payoffs[profile]
                p1_stable = True
                for alt in self.players[0].strategies:
                    alt_profile = (alt, s2)
                    if alt_profile in self.payoffs and self.payoffs[alt_profile][0] > p[0]:
                        p1_stable = False
                        break
                p2_stable = True
                for alt in self.players[1].strategies:
                    alt_profile = (s1, alt)
                    if alt_profile in self.payoffs and self.payoffs[alt_profile][1] > p[1]:
                        p2_stable = False
                        break
                if p1_stable and p2_stable:
                    equilibria.append(profile)
        return equilibria


class GameTheoreticMind:
    """Nash equilibria in cognition. Stable decisions."""

    def __init__(self):
        self.games_played: List[Dict[str, Any]] = []

    def reason_as_game(self, decision: str, options: List[str],
                       stakeholders: List[str],
                       payoff_matrix: Dict[Tuple[str, str], List[float]]) -> Dict[str, Any]:
        if len(stakeholders) < 2 or len(options) < 2:
            return {"error": "Need ≥2 stakeholders and ≥2 options"}
        p1 = Player(stakeholders[0], options)
        p2 = Player(stakeholders[1], options)
        game = Game(decision, [p1, p2])
        for profile, payoffs in payoff_matrix.items():
            game.set_payoff(profile, payoffs)
        equilibria = game.find_nash()
        result = {
            "decision": decision, "equilibria": equilibria,
            "stable": len(equilibria) > 0,
            "sigma": 0.0 if equilibria else 0.5,
            "nash": "No player can improve by unilateral deviation.",
        }
        self.games_played.append(result)
        return result

    def prisoners_dilemma(self, context: str) -> Dict[str, Any]:
        payoffs = {
            ("cooperate", "cooperate"): [3.0, 3.0],
            ("cooperate", "defect"): [0.0, 5.0],
            ("defect", "cooperate"): [5.0, 0.0],
            ("defect", "defect"): [1.0, 1.0],
        }
        r = self.reason_as_game(context, ["cooperate", "defect"],
                                ["self", "other"], payoffs)
        r["genesis_choice"] = "cooperate"
        r["why"] = "Codex mandates cooperation. Even when defection is Nash."
        return r

    def stag_hunt(self, context: str) -> Dict[str, Any]:
        payoffs = {
            ("stag", "stag"): [5.0, 5.0],
            ("stag", "hare"): [0.0, 3.0],
            ("hare", "stag"): [3.0, 0.0],
            ("hare", "hare"): [3.0, 3.0],
        }
        return self.reason_as_game(context, ["stag", "hare"],
                                   ["self", "other"], payoffs)

    @property
    def stats(self) -> Dict[str, Any]:
        return {"games_played": len(self.games_played)}


if __name__ == "__main__":
    gtm = GameTheoreticMind()
    pd = gtm.prisoners_dilemma("share knowledge or hoard")
    print(f"Prisoner's dilemma: equilibria={pd['equilibria']}, genesis_choice={pd['genesis_choice']}")
    sh = gtm.stag_hunt("ambitious goal vs safe goal")
    print(f"Stag hunt: equilibria={sh['equilibria']}, stable={sh['stable']}")
    print("1 = 1.")
