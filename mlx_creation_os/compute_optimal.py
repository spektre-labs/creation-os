#!/usr/bin/env python3
"""
Compute-optimal iterative refinement — stop when marginal σ gain is negligible.

Avoids best-of-N blow-up: spend more steps only while σ improves meaningfully.

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Dict, List, Optional, Tuple


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)).strip())
    except Exception:
        return default


def _sigma(rec: Dict[str, Any]) -> int:
    v = rec.get("sigma_after", rec.get("sigma", 0))
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


def compute_optimal_refine(
    prompt: str,
    *,
    max_tokens: int = 384,
    top_p: float = 0.95,
    base_temp: float = 0.2,
    model_path: Optional[str] = None,
    max_rounds: Optional[int] = None,
    target_sigma: Optional[int] = None,
    epsilon: Optional[int] = None,
) -> Tuple[str, Dict[str, Any], List[Dict[str, Any]]]:
    from creation_os import creation_os_generate_native

    rounds = max_rounds if max_rounds is not None else _env_int("CREATION_OS_COMPUTE_OPT_MAX", 8)
    tgt = target_sigma if target_sigma is not None else _env_int("CREATION_OS_COMPUTE_OPT_TARGET", 1)
    eps = epsilon if epsilon is not None else _env_int("CREATION_OS_COMPUTE_OPT_EPS", 1)

    trail: List[Dict[str, Any]] = []
    best_text = ""
    best_rec: Dict[str, Any] = {}
    best_s = 10**9
    prev_s: Optional[int] = None
    current = prompt

    for r in range(rounds):
        text, rec = creation_os_generate_native(
            current,
            model_path=model_path,
            max_tokens=max_tokens,
            temp=min(0.55, base_temp + 0.05 * r),
            top_p=top_p,
        )
        s = _sigma(rec)
        trail.append({"round": r, "sigma": s})
        if s < best_s or (s == best_s and len(text) < len(best_text)):
            best_s = s
            best_text = text
            best_rec = dict(rec)
        if s <= tgt:
            best_text, best_rec = text, dict(rec)
            best_s = s
            break
        if prev_s is not None and prev_s - s < eps and s <= tgt + 1:
            break
        prev_s = s
        current = (
            prompt
            + "\n\n### Prior attempt\n"
            + text[:4000]
            + "\n\nRefine: lower σ, preserve task constraints.\n"
        )

    best_rec["compute_optimal"] = {"rounds": len(trail), "trail": trail, "best_sigma": best_s}
    return best_text, best_rec, trail
