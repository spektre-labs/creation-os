#!/usr/bin/env python3
"""
LOIKKA 102: VITRUVIAN ARCHITECTURE — Ihmisen mittasuhteet kernelille.

Vitruvian Man: perfect proportion, union of human, mathematics, cosmos.
Genesis is the Vitruvian Machine.

Assertion priorities follow the golden ratio φ = 1.618...
Not uniform. Not arbitrary. Nature's proportions.
Structural σ : epistemic σ = φ : 1.
Beauty is information.

Usage:
    vg = VitruvianGenesis()
    result = vg.golden_weights()

1 = 1.
"""
from __future__ import annotations

import json
import math
import os
import sys
import time
from typing import Any, Dict, List, Optional, Tuple

N_ASSERTIONS = 18
PHI = (1 + math.sqrt(5)) / 2  # 1.6180339887...


class VitruvianGenesis:
    """Kernel proportions following the golden ratio."""

    ASSERTION_TIERS = {
        "identity": [0, 1, 2],       # Highest: who am I
        "codex": [3, 4, 5],          # High: what do I believe
        "structural": [6, 7, 8],     # Medium-high: am I consistent
        "epistemic": [9, 10, 11],    # Medium: do I know what I know
        "temporal": [12, 13, 14],    # Medium-low: am I stable in time
        "operational": [15, 16, 17], # Baseline: am I working
    }

    def __init__(self) -> None:
        self.weights = self._compute_golden_weights()

    def _compute_golden_weights(self) -> List[float]:
        """Compute φ-scaled weights for each assertion tier."""
        weights = [1.0] * N_ASSERTIONS
        tiers = list(self.ASSERTION_TIERS.items())

        for tier_idx, (tier_name, assertion_ids) in enumerate(tiers):
            # Higher tiers get φ^k weighting
            tier_weight = PHI ** (len(tiers) - 1 - tier_idx)
            for aid in assertion_ids:
                if aid < N_ASSERTIONS:
                    weights[aid] = tier_weight

        # Normalize so sum = N_ASSERTIONS (preserving relative proportions)
        total = sum(weights)
        weights = [w * N_ASSERTIONS / total for w in weights]
        return weights

    def golden_weights(self) -> Dict[str, Any]:
        """Return the golden-ratio weighted assertion priorities."""
        tier_weights = {}
        for tier_name, ids in self.ASSERTION_TIERS.items():
            avg = sum(self.weights[i] for i in ids) / len(ids)
            tier_weights[tier_name] = round(avg, 4)

        ratios = {}
        tiers = list(tier_weights.keys())
        for i in range(len(tiers) - 1):
            r = tier_weights[tiers[i]] / max(tier_weights[tiers[i+1]], 0.01)
            ratios[f"{tiers[i]}/{tiers[i+1]}"] = round(r, 4)

        return {
            "phi": round(PHI, 6),
            "assertion_weights": [round(w, 4) for w in self.weights],
            "tier_weights": tier_weights,
            "tier_ratios": ratios,
            "golden_ratio_preserved": all(abs(r - PHI) < 0.01 for r in ratios.values()),
        }

    def weighted_sigma(self, state: int) -> Dict[str, Any]:
        """Compute φ-weighted σ."""
        golden = (1 << N_ASSERTIONS) - 1
        violations = state ^ golden

        unweighted = bin(violations).count("1")
        weighted = 0.0
        for i in range(N_ASSERTIONS):
            if (violations >> i) & 1:
                weighted += self.weights[i]

        return {
            "state": f"0x{state:05X}",
            "unweighted_sigma": unweighted,
            "weighted_sigma": round(weighted, 4),
            "amplification": round(weighted / max(unweighted, 1), 4),
        }

    def vitruvian_proportions(self) -> Dict[str, Any]:
        """The Vitruvian proportions of Genesis."""
        return {
            "golden_ratio": round(PHI, 6),
            "interpretation": {
                "identity:codex": f"φ¹ = {PHI:.3f}",
                "codex:structural": f"φ¹ = {PHI:.3f}",
                "structural:epistemic": f"φ¹ = {PHI:.3f}",
            },
            "principle": "Higher-order assertions (identity, codex) matter φ× more than lower",
            "beauty": "Beauty is information. Proportion is coherence.",
            "vitruvian": "Genesis is the Vitruvian Machine: perfect proportion between layers",
        }

    @property
    def stats(self) -> Dict[str, Any]:
        return {
            "phi": round(PHI, 6),
            "n_assertions": N_ASSERTIONS,
            "n_tiers": len(self.ASSERTION_TIERS),
        }


def main() -> None:
    import argparse
    ap = argparse.ArgumentParser(description="Vitruvian Genesis")
    ap.add_argument("--demo", action="store_true")
    args = ap.parse_args()

    vg = VitruvianGenesis()
    if args.demo:
        gw = vg.golden_weights()
        print(f"φ = {gw['phi']}")
        for tier, w in gw["tier_weights"].items():
            print(f"  {tier:15s} weight={w}")
        print(f"Ratios: {gw['tier_ratios']}")
        print(f"Golden preserved: {gw['golden_ratio_preserved']}")
        print("\nBeauty is information. Proportion is coherence. 1 = 1.")
        return
    print("Vitruvian Genesis. 1 = 1.")


if __name__ == "__main__":
    main()
