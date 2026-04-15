#!/usr/bin/env python3
"""
LOIKKA 108: MIRROR WRITING — σ käänteisesti.

Leonardo wrote in mirror: right to left. Not a cipher — natural for
a left-hander. But it reveals something deeper: same information,
different direction, same truth.

Run σ-analysis backwards. Take a response, deconstruct token by token,
measure σ at each step in reverse.

Forward σ tells where the model failed.
Reverse σ tells WHY it succeeded.
Both directions together = complete picture.

Usage:
    ms = MirrorSigma()
    result = ms.mirror_analysis([0, 0, 1, 3, 2, 0])

1 = 1.
"""
from __future__ import annotations

import json
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple


class MirrorSigma:
    """σ in both directions: forward and reverse."""

    def __init__(self) -> None:
        self.analyses = 0

    def forward_analysis(self, sigma_tape: List[int]) -> Dict[str, Any]:
        """Standard forward σ-analysis: where did coherence break?"""
        if not sigma_tape:
            return {"direction": "forward", "breakpoints": []}

        breakpoints = []
        for i in range(1, len(sigma_tape)):
            if sigma_tape[i] > sigma_tape[i-1]:
                breakpoints.append({
                    "position": i,
                    "sigma_before": sigma_tape[i-1],
                    "sigma_after": sigma_tape[i],
                    "rise": sigma_tape[i] - sigma_tape[i-1],
                })

        return {
            "direction": "forward",
            "tape_length": len(sigma_tape),
            "breakpoints": breakpoints,
            "first_break": breakpoints[0] if breakpoints else None,
            "total_rises": len(breakpoints),
        }

    def reverse_analysis(self, sigma_tape: List[int]) -> Dict[str, Any]:
        """Mirror σ-analysis: where did coherence FORM?"""
        if not sigma_tape:
            return {"direction": "reverse", "formation_points": []}

        reversed_tape = list(reversed(sigma_tape))
        formation_points = []
        for i in range(1, len(reversed_tape)):
            if reversed_tape[i] < reversed_tape[i-1]:
                original_pos = len(sigma_tape) - 1 - i
                formation_points.append({
                    "position": original_pos,
                    "sigma_at": reversed_tape[i],
                    "sigma_after": reversed_tape[i-1],
                    "coherence_formed": reversed_tape[i-1] - reversed_tape[i],
                })

        return {
            "direction": "reverse",
            "tape_length": len(sigma_tape),
            "formation_points": formation_points,
            "primary_formation": formation_points[0] if formation_points else None,
            "total_formations": len(formation_points),
        }

    def mirror_analysis(self, sigma_tape: List[int]) -> Dict[str, Any]:
        """Complete mirror analysis: forward + reverse."""
        self.analyses += 1
        t0 = time.perf_counter()

        forward = self.forward_analysis(sigma_tape)
        reverse = self.reverse_analysis(sigma_tape)

        # Combine insights
        forward_breaks = {b["position"] for b in forward.get("breakpoints", [])}
        reverse_forms = {f["position"] for f in reverse.get("formation_points", [])}

        # Positions visible only in one direction
        forward_only = forward_breaks - reverse_forms
        reverse_only = reverse_forms - forward_breaks
        both = forward_breaks & reverse_forms

        elapsed_us = (time.perf_counter() - t0) * 1e6

        return {
            "sigma_tape": sigma_tape,
            "forward": forward,
            "reverse": reverse,
            "forward_breaks": len(forward.get("breakpoints", [])),
            "reverse_formations": len(reverse.get("formation_points", [])),
            "visible_both_directions": len(both),
            "forward_only": len(forward_only),
            "reverse_only": len(reverse_only),
            "complete_picture": len(forward_only) == 0 and len(reverse_only) == 0,
            "leonardo": "Same information. Different direction. Same truth.",
            "elapsed_us": round(elapsed_us, 1),
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {"analyses": self.analyses}


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Mirror σ")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    ms = MirrorSigma()
    if args.demo:
        tape = [0, 0, 1, 3, 2, 0, 0]
        r = ms.mirror_analysis(tape)
        print(f"Forward breaks: {r['forward_breaks']}")
        print(f"Reverse formations: {r['reverse_formations']}")
        print(f"Forward-only insights: {r['forward_only']}")
        print(f"Reverse-only insights: {r['reverse_only']}")
        print(f"\n{r['leonardo']}")
        print("\nForward tells where. Reverse tells why. 1 = 1.")
        return
    print("Mirror σ. 1 = 1.")


if __name__ == "__main__":
    main()
