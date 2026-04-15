#!/usr/bin/env python3
"""
Two-layer TTS: majority vote aggregation, then σ self-verification.

When plurality picks an answer but σ remains high, run budget forcing / extra
refinement — performance can improve **after** vote saturation (research theme).

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Dict, Optional, Tuple

from adaptive_tts import majority_vote_generate


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


def self_verify_majority_generate(
    prompt: str,
    *,
    n: int = 5,
    max_tokens: int = 384,
    top_p: float = 0.95,
    model_path: Optional[str] = None,
    sigma_post_threshold: Optional[int] = None,
) -> Tuple[str, Dict[str, Any]]:
    text, rec, meta = majority_vote_generate(
        prompt, n=n, max_tokens=max_tokens, top_p=top_p, model_path=model_path
    )
    thr = sigma_post_threshold if sigma_post_threshold is not None else _env_int("CREATION_OS_SVM_POST_SIGMA", 4)
    s0 = _sigma(rec)
    rec = dict(rec)
    rec["self_verify_majority"] = {"phase": "vote", "sigma": s0, "meta": meta}

    if s0 <= thr:
        rec["self_verify_majority"]["phase2"] = "skipped_low_sigma"
        return text, rec

    from budget_forcing import budget_forcing_generate

    refine_prompt = (
        prompt
        + "\n\n### Majority-chosen draft (may be imperfect)\n"
        + (text or "")[:3500]
        + "\n\nImprove this draft to reduce σ and satisfy the task format.\n"
    )
    text2, rec2, trail = budget_forcing_generate(
        refine_prompt,
        max_tokens=max_tokens,
        top_p=top_p,
        model_path=model_path,
        sigma_target=max(0, thr - 1),
    )
    s1 = _sigma(rec2)
    if s1 < s0 or (s1 == s0 and len(text2) < len(text)):
        rec2 = dict(rec2)
        rec2["self_verify_majority"] = {
            "phase": "vote_then_budget_forcing",
            "sigma_vote": s0,
            "sigma_after": s1,
            "vote_meta": meta,
            "bf_trail": trail,
        }
        return text2, rec2

    rec["self_verify_majority"]["phase2"] = "kept_vote_refinement_not_better"
    return text, rec
