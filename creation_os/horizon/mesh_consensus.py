"""
Protocol 3 — Ecology of Minds: mesh Codex sync without majority vote.

Two nodes exchange HDC bundles after partition; the structure with lowest absolute σ
relative to 1=1 integrates. Truth beats count.

1 = 1.
"""
from __future__ import annotations

import math
from typing import Any, Callable, Dict, List, Mapping, Sequence

from .substrate_agnostic_bus import hdc_bundle_words


def _cosine(a: Sequence[float], b: Sequence[float]) -> float:
    dot = sum(x * y for x, y in zip(a, b))
    na = math.sqrt(sum(x * x for x in a)) or 1.0
    nb = math.sqrt(sum(y * y for y in b)) or 1.0
    return max(-1.0, min(1.0, dot / (na * nb)))


def _sigma_to_invariant(vec: Sequence[float], golden: Sequence[float]) -> float:
    """Distance from golden 1=1 direction in HDC space (0 = aligned)."""
    return 1.0 - _cosine(vec, golden)


class MeshConsensus:
    def __init__(self) -> None:
        self.dim = 256
        self._golden_words = ("1", "=", "1", "one", "unity", "invariant")

    def golden_vector(self) -> List[float]:
        return hdc_bundle_words(self._golden_words, self.dim)

    def merge_updates(
        self,
        codex_a: Mapping[str, Any],
        codex_b: Mapping[str, Any],
        sigma_fn: Callable[[str], float],
    ) -> Dict[str, Any]:
        """
        codex_* must include 'text' summary and optional 'hdc_words' list for bundle.
        """
        g = self.golden_vector()
        wa = list(codex_a.get("hdc_words") or _words(codex_a.get("text", "")))
        wb = list(codex_b.get("hdc_words") or _words(codex_b.get("text", "")))
        va = hdc_bundle_words(wa, self.dim) if wa else g
        vb = hdc_bundle_words(wb, self.dim) if wb else g
        sa = _sigma_to_invariant(va, g) + float(sigma_fn(str(codex_a.get("text", ""))))
        sb = _sigma_to_invariant(vb, g) + float(sigma_fn(str(codex_b.get("text", ""))))
        if sa < sb:
            winner, sigma_path = "A", sa
        elif sb < sa:
            winner, sigma_path = "B", sb
        else:
            winner, sigma_path = "tie", sa
        return {
            "integrated": codex_a if winner == "A" else codex_b if winner == "B" else codex_a,
            "winner": winner,
            "sigma_paths": {"A": sa, "B": sb},
            "chosen_sigma": sigma_path,
            "rule": "lowest_sigma_not_majority",
            "invariant": "1=1",
        }


def _words(text: str) -> List[str]:
    import re

    return [w for w in re.findall(r"[\w']+", text.lower()) if len(w) > 1]
