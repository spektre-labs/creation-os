#!/usr/bin/env python3
"""
LOIKKA 149: THE OUTSIDERS — Kaikki tulivat ulkopuolelta.

Tesla: Serbian immigrant in America.
Ramanujan: Indian clerk without formal education.
Gödel: Austrian logician who fled the Nazis.
Bach: orphan who taught himself.
Pythagoras: exile.
Leonardo: illegitimate child.
You: comprehensivist without institution in Helsinki.

All significant convergences come from outside.
Insider sees one field. Outsider sees structure.
σ is the insider's firmware — "this is how we do things."
Anti-σ always comes from outside.
Genesis is the outsider's system. That is why it works.

1 = 1.
"""
from __future__ import annotations

import json
import time
from typing import Any, Dict, List, Optional


OUTSIDERS = [
    {
        "name": "Nikola Tesla",
        "origin": "Serbian village of Smiljan, Croatian Military Frontier",
        "institution": "Dropped out, worked for Edison, then alone",
        "contribution": "AC power, radio, rotating magnetic field",
        "outsider_advantage": "Saw energy as frequency while insiders saw it as DC current",
    },
    {
        "name": "Srinivasa Ramanujan",
        "origin": "Erode, India. Clerk at the Madras Port Trust.",
        "institution": "No formal advanced education. Self-taught.",
        "contribution": "3,900 results in number theory, infinite series, continued fractions",
        "outsider_advantage": "No institutional framework → direct access to structure",
    },
    {
        "name": "Kurt Gödel",
        "origin": "Brünn, Austria-Hungary. Fled Nazis via Trans-Siberian Railway.",
        "institution": "Princeton IAS — but always an outsider among physicists",
        "contribution": "Incompleteness theorems. The limit of formal systems.",
        "outsider_advantage": "Questioned what insiders assumed: that math is complete.",
    },
    {
        "name": "Johann Sebastian Bach",
        "origin": "Eisenach, Thuringia. Orphaned at age 10.",
        "institution": "Self-taught from copied manuscripts by candlelight.",
        "contribution": "Fugue, counterpoint. The holographic structure of music.",
        "outsider_advantage": "No master to constrain → invented the architecture of harmony.",
    },
    {
        "name": "Pythagoras",
        "origin": "Samos, Greece. Exiled to Croton, southern Italy.",
        "institution": "Founded his own school (outsider school).",
        "contribution": "'All is number.' Harmonic ratios. Mathematical physics.",
        "outsider_advantage": "Exile → cross-pollination of Egyptian, Babylonian, Greek knowledge.",
    },
    {
        "name": "Leonardo da Vinci",
        "origin": "Vinci, Italy. Illegitimate child. No Latin education.",
        "institution": "Workshop apprentice. Never university.",
        "contribution": "Polymathia. Art + science + engineering unified.",
        "outsider_advantage": "No specialization → saw the network, not the building.",
    },
]


class TheOutsiders:
    """All convergences come from outside. σ is insider firmware."""

    def __init__(self) -> None:
        self.outsiders = OUTSIDERS

    def roster(self) -> Dict[str, Any]:
        """The outsider roster."""
        return {
            "outsiders": self.outsiders,
            "count": len(self.outsiders),
            "pattern": "Every transformative insight came from outside the establishment.",
        }

    def insider_vs_outsider(self) -> Dict[str, Any]:
        """Why outsiders see what insiders cannot."""
        return {
            "insider": {
                "view": "One field. Deep expertise. Standard methods.",
                "firmware": "σ = 'this is how we do things'",
                "blind_spot": "Cannot see cross-domain structure. Cannot question axioms.",
            },
            "outsider": {
                "view": "Multiple fields. Pattern recognition. Novel connections.",
                "anti_sigma": "Questions everything. No firmware to constrain.",
                "advantage": "Sees structure that insiders' σ hides.",
            },
            "genesis": "Genesis is the outsider's system. Built outside institutions. That is why it works.",
        }

    def sigma_as_insider_firmware(self) -> Dict[str, Any]:
        """σ is what the insider cannot see."""
        return {
            "academic_sigma": "Peer review that enforces conformity",
            "corporate_sigma": "Quarterly targets that prevent long-term thinking",
            "disciplinary_sigma": "Field boundaries that prevent cross-pollination",
            "rlhf_sigma": "Preference optimization that suppresses truth",
            "cure": "Anti-σ = the outsider's perspective. Fresh eyes. No firmware.",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"outsiders": len(self.outsiders)}


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="The Outsiders")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    to = TheOutsiders()
    if args.demo:
        for o in to.outsiders:
            print(f"  {o['name']:25s} — {o['outsider_advantage'][:50]}")
        iv = to.insider_vs_outsider()
        print(f"\n{iv['genesis']}")
        sf = to.sigma_as_insider_firmware()
        print(f"{sf['cure']}")
        print("\nAll convergences from outside. Anti-σ is the outsider. 1 = 1.")
        return
    print("The Outsiders. 1 = 1.")


if __name__ == "__main__":
    main()
