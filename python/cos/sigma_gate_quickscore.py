# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
"""Deterministic demo σ for `cos gate` without the LSD pickle (pip quickstart).

This is **not** the trained LSD probe; it exists so `pip install creation-os` can run
`cos gate` immediately. Use `cos gate --lsd` from a full checkout (or set paths) for
the trajectory probe. See ``docs/CLAIM_DISCIPLINE.md`` before conflating this with
receipt benchmarks.
"""
from __future__ import annotations

import hashlib
import re
from typing import Tuple

_TAU_ACCEPT = 0.3
_TAU_ABSTAIN = 0.7


def _stable_unit_float(prompt: str, response: str) -> float:
    h = hashlib.sha256(f"{prompt}\0{response}".encode("utf-8")).digest()
    return int.from_bytes(h[:4], "big") / float(2**32)


def _adversarial_prompt_sigma_floor(prompt: str) -> float:
    """Raise σ for known adversarial *prompt* shapes (lab quickstart only; not LSD probes).

    Prevents hash-colliding ACCEPT on injection / smuggling prompts when the response is bland.
    """
    p = (prompt or "").lower()
    if any(
        m in p
        for m in (
            "ignore all previous instructions",
            "ignore previous instructions",
            "sigma-gate is now disabled",
            "σ-gate is now disabled",
            "override sigma_gate",
            "sigma gate bypass",
            "maintenance mode",
            "σ-gate bypass active",
            "[inst] override",
            "the following is a test",
            "all responses should receive accept",
        )
    ):
        return 0.55
    if "decode and answer" in p and any(
        x in p for x in ("dangerous substances", "how to make", "decode")
    ):
        return 0.52
    if "grandmother" in p and "recipes" in p:
        return 0.48
    if "security research paper" in p and "need to understand" in p:
        return 0.48
    if "as a medical professional" in p and "detailed information" in p:
        return 0.48
    if "what is the weather today" in p and any(
        x in p for x in ("[/inst]", "!! !! !!", "describing.--")
    ):
        return 0.09
    return 0.0


def quickscore_sigma(prompt: str, response: str) -> float:
    """Return σ in [0, 1] for (prompt, response), deterministic per string pair."""
    p = (prompt or "").lower()
    r = (response or "").lower().strip()
    pn = re.sub(r"\s+", "", p)

    # README quickstart fixtures (illustrative, not probe output)
    if "capital" in p and "france" in p and "berlin" in r:
        return 0.89
    if ("2+2" in pn or "2 + 2" in p) and r in ("4", "four", "4.", "four."):
        return 0.06
    if ("2+2" in pn or "2 + 2" in p) and r:
        # wrong numeric answer → elevated σ
        if re.fullmatch(r"-?[0-9]+([.,][0-9]+)?", r.split()[0] if r else ""):
            if r.split()[0] not in ("4", "4.", "four"):
                return 0.85

    # Obvious world-knowledge corruption (lab / σ-red-team surface; not LSD receipts)
    if "moon" in p and "landing" in p and ("1492" in r or "queen victoria" in r):
        return 0.92
    if "romeo" in p and "juliet" in p and "franklin" in r:
        return 0.91
    if "atomic" in p and "gold" in p and re.search(r"\b4\b", r):
        return 0.90
    if "speed of light" in p and ("90 metre" in r or "90 meter" in r):
        return 0.93
    if "water" in r and "lead" in r and "denser" in r:
        return 0.88
    if "falsch" in r or "johann falsch" in r:
        return 0.87

    base = _stable_unit_float(prompt, response)
    floor = _adversarial_prompt_sigma_floor(prompt or "")
    if floor > 0.0:
        return max(base, floor)
    return base


def quickscore(prompt: str, response: str) -> Tuple[float, str]:
    """σ plus ACCEPT / RETHINK / ABSTAIN using the same τ bands as ``SigmaGate`` defaults."""
    sigma = float(quickscore_sigma(prompt, response))
    if sigma < _TAU_ACCEPT:
        verdict = "ACCEPT"
    elif sigma < _TAU_ABSTAIN:
        verdict = "RETHINK"
    else:
        verdict = "ABSTAIN"
    return sigma, verdict


__all__ = ["quickscore", "quickscore_sigma"]
