"""
Protocol 6 — Temporal decay: anchor language to mathematical invariants.

Critical concepts are tethered to φ, π, e, and 1=1 so drift across centuries
re-calibrates lexicon against constants, not fashion.

1 = 1.
"""
from __future__ import annotations

import math
from typing import Any, Dict, List, Mapping, MutableMapping, Sequence

# Absolute mathematical anchors (language-independent)
ANCHORS: Mapping[str, float] = {
    "PHI": (1.0 + math.sqrt(5.0)) / 2.0,
    "PI": math.pi,
    "E": math.e,
    "ONE": 1.0,
    "UNITY": 1.0,
    "INVARIANT": 1.0,
}


class OntologicalAnchor:
    def __init__(self) -> None:
        self._lexicon: MutableMapping[str, str] = {}

    def register_term(self, natural: str, anchor_key: str) -> None:
        if anchor_key not in ANCHORS:
            raise KeyError(anchor_key)
        self._lexicon[natural.lower()] = anchor_key

    def reanchor_lexicon(self, tokens: Sequence[str]) -> Dict[str, Any]:
        """Map tokens to nearest anchor labels; unknown tokens pass through."""
        out: List[Dict[str, Any]] = []
        for t in tokens:
            low = t.lower()
            key = self._lexicon.get(low, "UNANCHORED")
            val = ANCHORS.get(key) if key in ANCHORS else None
            out.append(
                {
                    "token": t,
                    "anchor": key,
                    "value": val,
                }
            )
        return {
            "anchored": out,
            "constants": dict(ANCHORS),
            "invariant": "1=1",
        }
