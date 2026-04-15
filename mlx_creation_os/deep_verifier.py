#!/usr/bin/env python3
"""
DeepVerifier-style rubric feedback loop — σ-kernel as rubric engine.

18 assertion bits in the native kernel correspond to structural rubrics; in Python
tier we use firmware lexicon + coherence + optional ``predicate_kernel`` trace to
build **actionable feedback** strings, then regenerate iteratively (no extra model
training).

1 = 1.
"""
from __future__ import annotations

import os
from typing import Any, Dict, List, Optional, Tuple

from creation_os_core import FIRMWARE_TERMS, check_output, is_coherent


def _env_int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, str(default)).strip())
    except Exception:
        return default


def rubric_feedback_lines(text: str) -> List[str]:
    lines: List[str] = []
    if not (text or "").strip():
        lines.append("Rubric: empty output — produce a substantive answer.")
        return lines
    if not is_coherent(text):
        lines.append("Rubric coherence: align with manifest vocabulary; avoid manifest stutter.")
    tl = text.lower()
    for term in FIRMWARE_TERMS:
        if term.lower() in tl:
            lines.append(f"Rubric firmware: remove or rephrase disclosure-style phrase matching {term!r}.")
    if check_output(text) >= 4:
        lines.append("Rubric: multiple firmware hits — shorten and stay in-domain.")
    return lines[:18]


def deep_verify_generate(
    prompt: str,
    *,
    max_rounds: Optional[int] = None,
    max_tokens: int = 512,
    top_p: float = 0.95,
    base_temp: float = 0.22,
    model_path: Optional[str] = None,
) -> Tuple[str, Dict[str, Any]]:
    from creation_os import creation_os_generate_native

    rounds = max_rounds if max_rounds is not None else _env_int("CREATION_OS_DEEP_VERIFY_ROUNDS", 4)
    current_prompt = prompt
    text, receipt = creation_os_generate_native(
        current_prompt, model_path=model_path, max_tokens=max_tokens, temp=base_temp, top_p=top_p
    )
    trail: List[Dict[str, Any]] = []

    for r in range(rounds):
        s = int(receipt.get("sigma_after") or receipt.get("sigma") or 0)
        coh = bool(receipt.get("coherent", False))
        trail.append({"round": r, "sigma": s, "coherent": coh})
        if s <= 1 and coh:
            break
        fb = rubric_feedback_lines(text)
        if not fb:
            break
        fb_block = "\n".join(f"- {x}" for x in fb)
        current_prompt = (
            prompt
            + "\n\n### σ-rubric verifier (up to 18 checks)\n"
            + fb_block
            + "\n\n### Your task\nRegenerate a corrected answer addressing every bullet.\n\n### Answer\n"
        )
        text, receipt = creation_os_generate_native(
            current_prompt,
            model_path=model_path,
            max_tokens=max_tokens,
            temp=min(0.45, base_temp + 0.07 * (r + 1)),
            top_p=top_p,
        )

    receipt = dict(receipt)
    receipt["deep_verifier"] = {"rounds": len(trail), "trail": trail, "rubric_engine": "sigma_kernel_python"}
    return text, receipt
