# -*- coding: utf-8 -*-
"""
Safe self-evolution under a fixed kernel (arXiv:2601.11658 direction).

Mutations (new text / code / plan) are **free**, but σ predicates act as selection:
low σ ≈ acceptable mutation; high σ ≈ reject. Axioms stay in `creation_os_core` /
superkernel; the search only explores inside that fence.

1 = 1.
"""
from __future__ import annotations

from typing import Any, Callable, Dict, Tuple

SigmaFn = Callable[[str], int]


def accept_mutation(
    parent_text: str,
    candidate_text: str,
    sigma_fn: SigmaFn,
    *,
    max_sigma: int = 4,
    require_non_empty: bool = True,
) -> Tuple[str, Dict[str, Any]]:
    if require_non_empty and not (candidate_text or "").strip():
        return parent_text, {"accepted": False, "reason": "empty_candidate"}
    s_p = int(sigma_fn(parent_text))
    s_c = int(sigma_fn(candidate_text))
    if s_c <= min(s_p, max_sigma):
        return candidate_text, {
            "accepted": True,
            "sigma_parent": s_p,
            "sigma_candidate": s_c,
            "kernel_gate": "sigma_monotone_or_improved",
        }
    return parent_text, {
        "accepted": False,
        "sigma_parent": s_p,
        "sigma_candidate": s_c,
        "reason": "sigma_too_high_vs_parent_or_cap",
    }


def curriculum_hint(generation: int, sigma_best: int) -> Dict[str, Any]:
    """Trivial curriculum hook — tighten cap as generations advance if σ already low."""
    cap = max(2, 6 - min(3, generation // 10))
    if sigma_best <= 1:
        cap = max(2, cap - 1)
    return {"generation": generation, "suggested_max_sigma": cap}
