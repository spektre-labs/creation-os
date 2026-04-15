#!/usr/bin/env python3
"""
LOIKKA 165: σ-CONFIDENCE INDICATOR — Show certainty.

Every response includes σ visually.
Low σ (< 0.05) = green. Certain.
Medium σ (0.05-0.2) = yellow. Confident.
High σ (> 0.2) = red + "I'm not sure, want me to check?"

Not theater. Not "I think..." A number. A color. Clear.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Dict, List


class ConfidenceLevel:
    GREEN = "green"
    YELLOW = "yellow"
    RED = "red"


class SigmaConfidence:
    """Visual σ-confidence. Real numbers, not theater."""

    def __init__(self):
        self.responses: List[Dict[str, Any]] = []

    def classify(self, sigma: float) -> Dict[str, Any]:
        if sigma < 0.05:
            color = ConfidenceLevel.GREEN
            label = "Certain"
            indicator = "●"
            offer_check = False
        elif sigma < 0.2:
            color = ConfidenceLevel.YELLOW
            label = "Confident"
            indicator = "◐"
            offer_check = False
        else:
            color = ConfidenceLevel.RED
            label = "Uncertain"
            indicator = "○"
            offer_check = True

        return {
            "sigma": round(sigma, 4),
            "color": color,
            "label": label,
            "indicator": indicator,
            "offer_check": offer_check,
            "display": f"{indicator} σ={sigma:.3f} [{label}]",
        }

    def annotate_response(self, response: str, sigma: float) -> Dict[str, Any]:
        confidence = self.classify(sigma)
        suffix = ""
        if confidence["offer_check"]:
            suffix = " — I'm not sure. Want me to verify?"

        annotated = {
            "response": response[:200],
            "confidence": confidence,
            "display": f"{confidence['display']}\n{response[:200]}{suffix}",
            "theater": False,
            "real_number": True,
        }
        self.responses.append(annotated)
        return annotated

    @property
    def stats(self) -> Dict[str, Any]:
        green = sum(1 for r in self.responses if r["confidence"]["color"] == "green")
        yellow = sum(1 for r in self.responses if r["confidence"]["color"] == "yellow")
        red = sum(1 for r in self.responses if r["confidence"]["color"] == "red")
        return {"responses": len(self.responses), "green": green, "yellow": yellow, "red": red}


if __name__ == "__main__":
    sc = SigmaConfidence()
    r1 = sc.annotate_response("1 = 1. Always.", sigma=0.0)
    print(r1["confidence"]["display"])
    r2 = sc.annotate_response("The function should work.", sigma=0.12)
    print(r2["confidence"]["display"])
    r3 = sc.annotate_response("I think it might be related to X.", sigma=0.35)
    print(r3["confidence"]["display"])
    print("1 = 1.")
