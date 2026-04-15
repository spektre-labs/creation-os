#!/usr/bin/env python3
"""
Budget forcing — extend internal reasoning when σ says "not done yet".

Research analogue: inject continuation (here: natural-language **Wait** instruction)
when the verifier (σ-kernel) exceeds a target, instead of accepting an early stop.
Integrates with small models (e.g. 3B) without retraining.

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Callable, Dict, List, Optional, Tuple

SigmaFn = Callable[[str], int]


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)).strip())
    except Exception:
        return default


def _sigma_from_receipt(receipt: Dict[str, Any]) -> int:
    v = receipt.get("sigma_after")
    if v is None:
        v = receipt.get("sigma")
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


WAIT_INSTRUCTION = (
    "\n\n[σ-gate / budget forcing]\n"
    "Wait — do not finalize yet. Continue step-by-step reasoning, then give the final answer "
    "in the format the original task required.\n"
)


def budget_forcing_generate(
    prompt: str,
    *,
    max_tokens: int = 512,
    top_p: float = 0.95,
    base_temp: float = 0.25,
    model_path: Optional[str] = None,
    sigma_target: Optional[int] = None,
    max_waits: Optional[int] = None,
    sigma_fn: Optional[SigmaFn] = None,
) -> Tuple[str, Dict[str, Any], List[Dict[str, Any]]]:
    """
    Repeatedly extend the prompt with prior draft + Wait instruction while σ > target.

    ``sigma_fn`` optional override (default: receipt σ after each pass).
    """
    from creation_os import creation_os_generate_native

    target = sigma_target if sigma_target is not None else _env_int("CREATION_OS_BF_SIGMA_TARGET", 2)
    waits = max_waits if max_waits is not None else _env_int("CREATION_OS_BF_MAX_WAITS", 5)

    trail: List[Dict[str, Any]] = []
    rolling_prompt = prompt
    best_text = ""
    best_receipt: Dict[str, Any] = {}
    best_sigma = 10**9

    for rnd in range(waits + 1):
        text, receipt = creation_os_generate_native(
            rolling_prompt,
            model_path=model_path,
            max_tokens=max_tokens,
            temp=base_temp if rnd == 0 else min(0.45, base_temp + 0.08 * rnd),
            top_p=top_p,
        )
        s = _sigma_from_receipt(receipt)
        if sigma_fn is not None:
            s = int(sigma_fn(text))
        trail.append({"round": rnd, "sigma": s, "chars": len(text)})
        if s < best_sigma or (s == best_sigma and len(text) < len(best_text)):
            best_sigma = s
            best_text = text
            best_receipt = dict(receipt)
        if s <= target:
            best_text, best_receipt = text, dict(receipt)
            break
        if rnd >= waits:
            break
        rolling_prompt = rolling_prompt + "\n\n### Partial reasoning so far\n" + text.strip() + WAIT_INSTRUCTION

    best_receipt["budget_forcing"] = {
        "rounds": len(trail),
        "sigma_target": target,
        "trail": trail,
        "final_sigma": _sigma_from_receipt(best_receipt),
    }
    return best_text, best_receipt, trail
